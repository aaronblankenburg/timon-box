#ifndef BOX_SYNC_H
#define BOX_SYNC_H

// Copying an album from the other box.
//
// Some albums only exist on one of the two boxes, so carrying the RFID token
// over finds an empty album. This header holds both halves of the fix:
//
//   Serving side  /peer-info, /peer-album, /peer-file. They look the album up
//                 by the token id from the query string, so the serving box
//                 only needs its management token, not the album token.
//   Fetching side A worker task pulls the file list, works out what is missing
//                 locally and writes the files to the SD card. The Copy page
//                 only queues jobs and polls /copy-status, so the transfer
//                 survives a page reload and a closed browser.
//
// Both boxes have to be on the home network; the peer is addressed by mDNS.
// Included at the end of management-server.h because it uses its helpers
// (filesLock, webServerFolderPath, renameFolder, the upload buffer pool, ...).

#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <SD.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "web-ui.h"
#include "wifi-setup.h"

#define SYNC_TEMP_FILE "/temp_sync"
#define SYNC_MAX_FILES 200          // cap on what one album may carry over
#define SYNC_CONNECT_TIMEOUT_MS 4000
#define SYNC_READ_TIMEOUT_MS 8000
#define SYNC_STALL_TIMEOUT_MS 10000  // no bytes for this long -> the transfer failed
#define SYNC_SPARE_BYTES (2u * 1024u * 1024u)  // keep this much free on the card

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

// Token ids are lowercase hex of the RFID uid. Rejecting anything else also
// stops an empty tag from matching the first directory via endsWith("").
bool isValidTag(const String &tag) {
  if (tag.length() < 8 || tag.length() > 20) return false;
  for (unsigned int i = 0; i < tag.length(); i++) {
    if (!isxdigit((unsigned char)tag[i])) return false;
  }
  return true;
}

// Track names travel in a query string and become file names, so keep them
// boring: no path separators, no leading dot, and mp3 only.
bool isValidTrackName(const String &name) {
  if (name.isEmpty() || name.length() > 100 || name[0] == '.') return false;
  if (name.indexOf('/') >= 0 || name.indexOf('\\') >= 0 || name.indexOf("..") >= 0) return false;
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".mp3");
}

String urlEncode(const String &in) {
  static const char *hex = "0123456789ABCDEF";
  String out;
  out.reserve(in.length() + 8);
  for (unsigned int i = 0; i < in.length(); i++) {
    char c = in[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      out += '%';
      out += hex[(c >> 4) & 0x0F];
      out += hex[c & 0x0F];
    }
  }
  return out;
}

// Read-only counterpart of Player::determineAlbumDir(): returns "" instead of
// creating a folder for an unknown token. Caller holds filesMutex.
String findAlbumFolder(const String &tag) {
  String found = "";
  File root = SD.open("/");
  if (!root) return found;
  while (File entry = root.openNextFile()) {
    if (entry.isDirectory() && String(entry.name()).endsWith(tag)) {
      found = "/" + String(entry.name());
      entry.close();
      break;
    }
    entry.close();
  }
  root.close();
  return found;
}

// ---------------------------------------------------------------------------
// Serving side (active whenever the management server runs)
// ---------------------------------------------------------------------------

void peerInfo(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", BOX_NAME "\n" + String(boxHostname()) + ".local\n1\n");
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

// One line "album\t<display name>", then one "<size>\t<name>" per track.
// Line based on purpose: the fetching box parses this without a JSON parser.
void peerAlbum(AsyncWebServerRequest *request) {
  String tag = request->hasParam("tag") ? request->getParam("tag")->value() : "";
  if (!isValidTag(tag)) {
    request->send(400, "text/plain", "bad token id");
    return;
  }
  if (!filesLock()) return sendBusy(request);

  String folder = findAlbumFolder(tag);
  if (folder.isEmpty()) {
    filesUnlock();
    request->send(404, "text/plain", "no album for this token");
    return;
  }

  AsyncResponseStream *res = request->beginResponseStream("text/plain", 2048);
  res->printf("album\t%s\n", folderDisplayName(folder, tag).c_str());
  int count = 0;
  File dir = SD.open(folder);
  while (File entry = dir.openNextFile()) {
    if (!entry.isDirectory() && String(entry.name()).endsWith(".mp3") && count < SYNC_MAX_FILES) {
      res->printf("%u\t%s\n", (unsigned)entry.size(), entry.name());
      count++;
    }
    entry.close();
  }
  dir.close();
  filesUnlock();

  Serial.printf("[peer] album %s for %s: %d files\n", folder.c_str(), tag.c_str(), count);
  request->send(res);
}

// The open File has to survive across filler calls, so it lives on the heap and
// is released in onDisconnect, which also fires after a completed response.
struct PeerFileCtx {
  File file;
  size_t size;
};

void peerFile(AsyncWebServerRequest *request) {
  String tag = request->hasParam("tag") ? request->getParam("tag")->value() : "";
  String name = request->hasParam("name") ? request->getParam("name")->value() : "";
  if (!isValidTag(tag) || !isValidTrackName(name)) {
    request->send(400, "text/plain", "bad token id or file name");
    return;
  }
  if (!filesLock()) return sendBusy(request);

  String folder = findAlbumFolder(tag);
  String path = folder + "/" + name;
  if (folder.isEmpty() || !SD.exists(path)) {
    filesUnlock();
    request->send(404, "text/plain", "no such track");
    return;
  }
  File file = SD.open(path, FILE_READ);
  if (!file) {
    filesUnlock();
    request->send(500, "text/plain", "could not open the file");
    return;
  }
  size_t size = file.size();
  filesUnlock();

  // Shared between the filler and the disconnect handler so the File outlives
  // whichever of the two goes away first.
  auto ctx = std::make_shared<PeerFileCtx>(PeerFileCtx{file, size});
  request->onDisconnect([ctx]() { ctx->file.close(); });

  // Own filler instead of request->send(SD, ...): AsyncFileResponse reads the
  // card unlocked from the AsyncTCP task, and in 3.7.10 it also drops the
  // content type we pass.
  AsyncWebServerResponse *response = request->beginResponse("audio/mpeg", size, [ctx](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    if (index >= ctx->size) return 0;
    if (!filesLock(50)) return RESPONSE_TRY_AGAIN;  // another task owns the card right now
    int read = ctx->file.read(buffer, std::min(maxLen, ctx->size - index));
    filesUnlock();
    return read > 0 ? (size_t)read : 0;
  });
  response->addHeader("Cache-Control", "no-store");
  Serial.printf("[peer] sending %s (%u bytes)\n", path.c_str(), (unsigned)size);
  request->send(response);
}

// ---------------------------------------------------------------------------
// Fetching side: state
// ---------------------------------------------------------------------------

enum SyncState { SYNC_IDLE,
                 SYNC_LISTING,
                 SYNC_LISTED,
                 SYNC_COPYING,
                 SYNC_DONE,
                 SYNC_FAILED,
                 SYNC_CANCELLED };
enum SyncFileStatus { SF_PENDING,
                      SF_PRESENT,
                      SF_COPYING,
                      SF_DONE,
                      SF_FAILED };
enum SyncOp { SYNC_OP_SCAN,
              SYNC_OP_LIST,
              SYNC_OP_COPY };

struct SyncFile {
  String name;
  uint32_t size;
  uint8_t status;
};

struct PeerBox {
  String name;
  String host;
};

struct SyncJob {
  SyncOp op;
  char host[64];
};

QueueHandle_t syncJobs = nullptr;
SemaphoreHandle_t syncMutex = nullptr;  // guards the vectors and strings below
std::vector<SyncFile> syncFiles;
std::vector<PeerBox> syncPeers;
String syncPeerInput;  // what the user typed, also stored in NVS
String syncPeerAddr;   // resolved "ip:port" or "host:port" used for the requests
String syncAlbumName;
String syncError;
volatile SyncState syncState = SYNC_IDLE;
volatile bool syncCancel = false;
volatile bool syncScanning = false;
volatile bool syncScanDone = false;
volatile uint32_t syncBytesDone = 0;   // completed files
volatile uint32_t syncBytesCur = 0;    // file currently in flight
volatile uint32_t syncBytesTotal = 0;  // all files that need copying
volatile int16_t syncCurrent = -1;
Preferences syncPrefs;

bool syncLock(uint32_t waitMs = 200) {
  return syncMutex && xSemaphoreTake(syncMutex, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}

void syncUnlock() { xSemaphoreGive(syncMutex); }

static void syncFail(const String &message) {
  if (syncLock(1000)) {
    syncError = message;
    syncUnlock();
  }
  syncState = SYNC_FAILED;
  Serial.println("[sync] " + message);
}

static bool syncBusy() {
  SyncState state = syncState;
  return state == SYNC_LISTING || state == SYNC_COPYING;
}

// ---------------------------------------------------------------------------
// Fetching side: worker task
// ---------------------------------------------------------------------------

// Accepts "julius-box", "julius-box.local", "192.168.0.157" and "host:8080".
static String resolvePeer(const String &input) {
  String host = input;
  host.trim();
  uint16_t port = 80;
  int colon = host.lastIndexOf(':');
  if (colon > 0) {
    port = host.substring(colon + 1).toInt();
    host = host.substring(0, colon);
    if (!port) port = 80;
  }
  String suffix = port == 80 ? "" : ":" + String(port);

  IPAddress ip;
  if (ip.fromString(host)) return host + suffix;

  String bare = host;
  if (bare.endsWith(".local")) bare = bare.substring(0, bare.length() - 6);
  IPAddress found = MDNS.queryHost(bare, 2000);
  if ((uint32_t)found != 0) {
    Serial.printf("[sync] resolving %s -> %s\n", bare.c_str(), found.toString().c_str());
    return found.toString() + suffix;
  }
  // lwIP does one-shot mDNS queries for .local itself, so this can still work.
  Serial.printf("[sync] mDNS did not answer for %s, trying the name directly\n", bare.c_str());
  return bare + ".local" + suffix;
}

static void syncScanPeers() {
  syncScanning = true;
  syncScanDone = false;
  int found = MDNS.queryService("http", "tcp");
  std::vector<PeerBox> boxes;
  for (int i = 0; i < found; i++) {
    if (!MDNS.hasTxt(i, "box")) continue;  // the TXT record marks one of our boxes
    String host = MDNS.hostname(i);
    if (host.equalsIgnoreCase(boxHostname())) continue;
    boxes.push_back({MDNS.txt(i, "box"), host + ".local"});
  }
  if (syncLock(1000)) {
    syncPeers = boxes;
    syncUnlock();
  }
  syncScanning = false;
  syncScanDone = true;
  Serial.printf("[sync] scan: %d http services, %d boxes\n", found, (int)boxes.size());
}

static void syncListAlbum(const String &input) {
  syncState = SYNC_LISTING;
  if (syncLock(1000)) {
    syncError = "";
    syncFiles.clear();
    syncAlbumName = "";
    syncUnlock();
  }

  String tag;
  std::vector<String> local;
  if (filesLock(5000)) {
    tag = webServerAlbumTag;
    local = filesInDir;
    filesUnlock();
  }
  if (tag.isEmpty()) {
    syncFail("No album token in this box.");
    return;
  }

  String addr = resolvePeer(input);
  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(SYNC_CONNECT_TIMEOUT_MS);
  http.setTimeout(SYNC_READ_TIMEOUT_MS);
  String url = "http://" + addr + "/peer-album?tag=" + tag;
  if (!http.begin(client, url)) {
    syncFail("Could not reach " + input + ".");
    return;
  }
  int code = http.GET();
  if (code == 404) {
    http.end();
    syncFail("The other box has no album for this token.");
    return;
  }
  if (code != 200) {
    http.end();
    syncFail(code > 0 ? "The other box answered " + String(code) + "." : "No answer from " + input + ". Is its management token in place and is it on the same network?");
    return;
  }

  String body = http.getString();
  http.end();

  std::vector<SyncFile> files;
  String album;
  uint32_t missingBytes = 0;
  int missing = 0;
  unsigned int pos = 0;
  while (pos < body.length() && files.size() < SYNC_MAX_FILES) {
    int nl = body.indexOf('\n', pos);
    if (nl < 0) nl = body.length();
    String line = body.substring(pos, nl);
    pos = nl + 1;
    if (line.endsWith("\r")) line.remove(line.length() - 1);
    int tab = line.indexOf('\t');
    if (tab < 1) continue;
    String head = line.substring(0, tab);
    String rest = line.substring(tab + 1);
    if (head == "album") {
      album = rest;
      continue;
    }
    if (!isValidTrackName(rest)) continue;
    bool present = std::find(local.begin(), local.end(), rest) != local.end();
    uint32_t size = strtoul(head.c_str(), nullptr, 10);
    files.push_back({rest, size, (uint8_t)(present ? SF_PRESENT : SF_PENDING)});
    if (!present) {
      missing++;
      missingBytes += size;
    }
  }

  syncPrefs.putString("peer", input);  // remember the box we last talked to
  if (syncLock(1000)) {
    syncFiles = files;
    syncAlbumName = album;
    syncPeerInput = input;
    syncPeerAddr = addr;
    syncUnlock();
  }
  syncBytesTotal = missingBytes;
  syncBytesDone = 0;
  syncBytesCur = 0;
  syncCurrent = -1;
  syncState = SYNC_LISTED;
  Serial.printf("[sync] album \"%s\", %d files, %d missing (%s)\n", album.c_str(), (int)files.size(), missing, formatBytes(missingBytes).c_str());
}

static void syncSetFileStatus(size_t index, uint8_t status) {
  if (!syncLock(1000)) return;
  if (index < syncFiles.size()) syncFiles[index].status = status;
  syncUnlock();
}

// Streams one track into SYNC_TEMP_FILE and moves it into the album folder.
static bool syncDownloadOne(const String &addr, const String &tag, const String &name, uint8_t *buffer, size_t bufferSize) {
  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(SYNC_CONNECT_TIMEOUT_MS);
  http.setTimeout(SYNC_READ_TIMEOUT_MS);
  String url = "http://" + addr + "/peer-file?tag=" + tag + "&name=" + urlEncode(name);
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != 200) {
    Serial.printf("[sync] %s: HTTP %d\n", name.c_str(), code);
    http.end();
    return false;
  }

  int expected = http.getSize();
  WiFiClient *stream = http.getStreamPtr();
  File out;
  if (filesLock(10000)) {
    SD.remove(SYNC_TEMP_FILE);
    out = SD.open(SYNC_TEMP_FILE, FILE_WRITE);
    filesUnlock();
  }
  if (!out) {
    http.end();
    Serial.println("[sync] could not open the temp file");
    return false;
  }

  bool ok = true;
  uint32_t got = 0;
  unsigned long startedAt = millis();
  unsigned long lastData = millis();
  unsigned long sdMs = 0;
  while (expected < 0 || got < (uint32_t)expected) {
    if (syncCancel) {
      ok = false;
      break;
    }
    size_t available = stream->available();
    if (!available) {
      if (!stream->connected()) {
        ok = expected > 0 && got == (uint32_t)expected;
        break;
      }
      if (millis() - lastData > SYNC_STALL_TIMEOUT_MS) {
        Serial.printf("[sync] %s: stalled after %u bytes\n", name.c_str(), (unsigned)got);
        ok = false;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }
    int read = stream->readBytes(buffer, std::min(available, bufferSize));
    if (read <= 0) continue;
    lastData = millis();
    unsigned long writeStart = millis();
    bool written = false;
    if (filesLock(10000)) {
      written = out.write(buffer, read) == (size_t)read;
      filesUnlock();
    }
    sdMs += millis() - writeStart;
    if (!written) {
      Serial.printf("[sync] %s: writing to the SD card failed\n", name.c_str());
      ok = false;
      break;
    }
    got += read;
    syncBytesCur = got;
  }
  http.end();

  if (ok && expected > 0 && got != (uint32_t)expected) ok = false;

  if (filesLock(10000)) {
    out.close();
    if (ok) {
      String target = webServerFolderPath + "/" + name;
      if (SD.exists(target)) SD.remove(target);  // overwrite an existing track
      ok = SD.rename(SYNC_TEMP_FILE, target);
      if (ok) {
        filesInDir.erase(std::remove(filesInDir.begin(), filesInDir.end(), name), filesInDir.end());
        filesInDir.push_back(name);
      }
    }
    if (!ok) SD.remove(SYNC_TEMP_FILE);
    filesUnlock();
  }

  unsigned long ms = millis() - startedAt;
  Serial.printf("[sync] %s %s, %u bytes in %lu ms (%.2f MB/s), SD busy %lu ms, heap %u\n",
                name.c_str(), ok ? "copied" : "failed", (unsigned)got, ms,
                ms ? got / 1048.576 / ms : 0.0, sdMs, ESP.getFreeHeap());
  return ok;
}

static void syncCopyAlbum() {
  syncState = SYNC_COPYING;
  syncCancel = false;
  syncBytesDone = 0;
  syncBytesCur = 0;

  std::vector<SyncFile> work;
  String addr, album;
  if (syncLock(1000)) {
    work = syncFiles;
    addr = syncPeerAddr;
    album = syncAlbumName;
    syncError = "";
    syncUnlock();
  }
  if (work.empty() || addr.isEmpty()) {
    syncFail("Load the album from the other box first.");
    return;
  }

  // Adopt the peer's album name while the local folder is still unnamed. Must
  // happen before the downloads so the files land in the final folder.
  String tag, folder;
  uint64_t freeBytes = 0;
  if (filesLock(10000)) {
    tag = webServerAlbumTag;
    folder = webServerFolderPath;
    if (!album.isEmpty() && folderDisplayName(folder, tag).isEmpty()) {
      String renamed = album;
      if (renameFolder(folder, renamed)) {
        webServerFolderPath = renamed;
        folder = renamed;
        prepareFilesInDir();
        Serial.println("[sync] album folder renamed to " + renamed);
      }
    }
    freeBytes = SD.totalBytes() > SD.usedBytes() ? SD.totalBytes() - SD.usedBytes() : 0;
    filesUnlock();
  }
  if (tag.isEmpty() || folder.isEmpty()) {
    syncFail("No album token in this box.");
    return;
  }

  uint32_t needed = 0;
  int pending = 0;
  for (const SyncFile &file : work) {
    if (file.status == SF_PENDING || file.status == SF_FAILED) {
      needed += file.size;
      pending++;
    }
  }
  syncBytesTotal = needed;
  if (!pending) {
    syncState = SYNC_DONE;
    Serial.println("[sync] nothing to copy");
    return;
  }
  if ((uint64_t)needed + SYNC_SPARE_BYTES > freeBytes) {
    syncFail("Not enough space on the SD card: " + formatBytes(needed) + " needed, " + formatBytes(freeBytes) + " free.");
    return;
  }

  // One 8 kB buffer borrowed from the upload pool; it is idle unless an upload
  // runs at the same time, in which case a small static buffer has to do.
  static uint8_t fallbackBuffer[1436];
  uint8_t *buffer = nullptr;
  bool borrowed = uploadFreeBufs && xQueueReceive(uploadFreeBufs, &buffer, pdMS_TO_TICKS(100)) == pdTRUE;
  size_t bufferSize = borrowed ? UPLOAD_BUF_SIZE : sizeof(fallbackBuffer);
  if (!borrowed) buffer = fallbackBuffer;

  Serial.printf("[sync] copying %d files (%s) from %s, heap %u, largest block %u\n",
                pending, formatBytes(needed).c_str(), addr.c_str(), ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  int copied = 0, failed = 0;
  for (size_t i = 0; i < work.size(); i++) {
    if (syncCancel) break;
    if (work[i].status != SF_PENDING && work[i].status != SF_FAILED) continue;
    syncCurrent = i;
    syncBytesCur = 0;
    syncSetFileStatus(i, SF_COPYING);
    bool ok = syncDownloadOne(addr, tag, work[i].name, buffer, bufferSize);
    syncSetFileStatus(i, ok ? SF_DONE : SF_FAILED);
    if (ok) {
      copied++;
      syncBytesDone += work[i].size;
    } else {
      failed++;
    }
    syncBytesCur = 0;
  }
  syncCurrent = -1;

  if (borrowed) xQueueSend(uploadFreeBufs, &buffer, 0);

  if (filesLock(10000)) {
    deleteIndexFiles(webServerFolderPath);  // the player rebuilds its playlist
    prepareFilesInDir();
    filesUnlock();
  }

  if (syncCancel) {
    syncState = SYNC_CANCELLED;
  } else if (failed) {
    if (syncLock(1000)) {
      syncError = String(failed) + " of " + String(pending) + " files could not be copied.";
      syncUnlock();
    }
    syncState = SYNC_FAILED;
  } else {
    syncState = SYNC_DONE;
  }
  Serial.printf("[sync] done: %d copied, %d failed, heap %u\n", copied, failed, ESP.getFreeHeap());
}

static void syncTask(void *) {
  SyncJob job;
  for (;;) {
    if (xQueueReceive(syncJobs, &job, portMAX_DELAY) != pdTRUE) continue;
    switch (job.op) {
      case SYNC_OP_SCAN: syncScanPeers(); break;
      case SYNC_OP_LIST: syncListAlbum(String(job.host)); break;
      case SYNC_OP_COPY: syncCopyAlbum(); break;
    }
  }
}

volatile bool syncTaskWanted = false;
TaskHandle_t syncTaskHandle = nullptr;

static bool syncQueue(SyncOp op, const String &host = "") {
  if (!syncJobs) return false;
  SyncJob job;
  job.op = op;
  snprintf(job.host, sizeof(job.host), "%s", host.c_str());
  if (xQueueSend(syncJobs, &job, 0) != pdTRUE) return false;
  syncTaskWanted = true;  // the loop task starts the worker, the job waits in the queue
  return true;
}

// Called from managementServerLoop(). The worker costs 6 kB of stack, which the
// pages need to render, so it is only created once someone actually copies.
void syncLoop() {
  if (!syncTaskWanted || syncTaskHandle) return;
  syncTaskWanted = false;
  if (xTaskCreatePinnedToCore(syncTask, "boxsync", 6144, nullptr, 2, &syncTaskHandle, 1) != pdPASS) {
    syncTaskHandle = nullptr;
    syncFail("Not enough memory to start the transfer.");
    return;
  }
  Serial.printf("[sync] worker started, heap %u\n", ESP.getFreeHeap());
}

// Call once from setupWebServer(); the worker itself starts on demand.
void syncBegin() {
  syncMutex = xSemaphoreCreateMutex();
  syncJobs = xQueueCreate(4, sizeof(SyncJob));
  syncPrefs.begin("sync", false);
  syncPeerInput = syncPrefs.getString("peer", "");
}

// ---------------------------------------------------------------------------
// Fetching side: web handlers
// ---------------------------------------------------------------------------

static const char *syncStateName(SyncState state) {
  switch (state) {
    case SYNC_LISTING: return "listing";
    case SYNC_LISTED: return "listed";
    case SYNC_COPYING: return "copying";
    case SYNC_DONE: return "done";
    case SYNC_FAILED: return "failed";
    case SYNC_CANCELLED: return "cancelled";
    default: return "idle";
  }
}

static const char *syncFileStatusName(uint8_t status) {
  switch (status) {
    case SF_PRESENT: return "present";
    case SF_COPYING: return "copying";
    case SF_DONE: return "done";
    case SF_FAILED: return "failed";
    default: return "pending";
  }
}

void copyStatusJson(AsyncWebServerRequest *request) {
  SyncState state = syncState;
  AsyncResponseStream *res = request->beginResponseStream("application/json", 4096);
  res->printf("{\"state\":\"%s\",\"station\":%s,\"bytes\":%u,\"bytesTotal\":%u,\"current\":%d,\"heap\":%u",
              syncStateName(state),
              netMode == NET_STATION && WiFi.status() == WL_CONNECTED ? "true" : "false",
              (unsigned)(syncBytesDone + syncBytesCur), (unsigned)syncBytesTotal, (int)syncCurrent, ESP.getFreeHeap());

  int total = 0, missing = 0, done = 0, failed = 0;
  if (syncLock(500)) {
    res->printf(",\"peer\":\"%s\",\"album\":\"%s\",\"error\":\"%s\"",
                jsonEscape(syncPeerInput).c_str(), jsonEscape(syncAlbumName).c_str(), jsonEscape(syncError).c_str());
    res->print(",\"files\":[");
    for (size_t i = 0; i < syncFiles.size(); i++) {
      const SyncFile &file = syncFiles[i];
      total++;
      if (file.status == SF_PENDING || file.status == SF_COPYING) missing++;
      if (file.status == SF_DONE) done++;
      if (file.status == SF_FAILED) failed++;
      if (i) res->print(',');
      res->printf("{\"name\":\"%s\",\"size\":%u,\"status\":\"%s\"}",
                  jsonEscape(file.name).c_str(), (unsigned)file.size, syncFileStatusName(file.status));
    }
    res->print("]");
    syncUnlock();
  } else {
    res->print(",\"peer\":\"\",\"album\":\"\",\"error\":\"\",\"files\":[]");
  }
  res->printf(",\"total\":%d,\"missing\":%d,\"done\":%d,\"failed\":%d}", total, missing, done, failed);

  AsyncWebServerResponse *response = res;
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

void copyScanJson(AsyncWebServerRequest *request) {
  if (!syncBusy() && (request->hasParam("rescan") || (!syncScanDone && !syncScanning))) {
    syncQueue(SYNC_OP_SCAN);
  }
  const char *state = syncBusy() ? "busy" : (syncScanning || !syncScanDone) ? "scanning" : "done";

  AsyncResponseStream *res = request->beginResponseStream("application/json", 1024);
  res->printf("{\"state\":\"%s\",\"boxes\":[", state);
  if (syncLock(500)) {
    for (size_t i = 0; i < syncPeers.size(); i++) {
      if (i) res->print(',');
      res->printf("{\"name\":\"%s\",\"host\":\"%s\"}", jsonEscape(syncPeers[i].name).c_str(), jsonEscape(syncPeers[i].host).c_str());
    }
    syncUnlock();
  }
  res->print("]}");

  AsyncWebServerResponse *response = res;
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

void copyListPost(AsyncWebServerRequest *request) {
  String peer = request->hasParam("peer", true) ? request->getParam("peer", true)->value() : "";
  peer.trim();
  if (peer.isEmpty() || peer.length() > 60) {
    request->redirect("/copy?err=1");
    return;
  }
  if (syncBusy()) {
    request->redirect("/copy?err=2");
    return;
  }
  syncQueue(SYNC_OP_LIST, peer);  // the worker stores it in NVS once it answers
  request->redirect("/copy");
}

void copyStartPost(AsyncWebServerRequest *request) {
  if (syncBusy() || syncState == SYNC_IDLE) {
    request->redirect("/copy?err=2");
    return;
  }
  syncQueue(SYNC_OP_COPY);
  request->redirect("/copy");
}

void copyCancelPost(AsyncWebServerRequest *request) {
  syncCancel = true;
  request->redirect("/copy");
}

void copyPage(AsyncWebServerRequest *request) {
  if (!filesLock()) return sendBusy(request);
  String folder = webServerFolderPath;
  filesUnlock();
  if (folder.isEmpty()) return request->redirect("/wifi");

  bool station = netMode == NET_STATION && WiFi.status() == WL_CONNECTED;
  String peer;
  if (syncLock(500)) {
    peer = syncPeerInput;
    syncUnlock();
  }

  AsyncResponseStream *res = request->beginResponseStream("text/html", 6144);
  pageHead(res, request, "copy");
  res->printf("<span id=\"cf\" hidden data-station=\"%d\"></span>", station ? 1 : 0);

  if (!station) {
    res->print("<div class=\"notice\"><i class=\"ic ic-warn\"></i><span><b>Home network needed.</b> Copying works only while both boxes are on the home network. "
               "This box is running its own hotspot, so set up Wi-Fi first.</span></div>");
  }
  res->print("<div id=\"cn\"></div>");

  // Card 1: pick the other box
  res->print("<div class=\"card\"><div class=\"card-head\"><h2>Other Box</h2>"
             "<button class=\"btn secondary small\" type=\"button\" id=\"ps\">Search</button></div>"
             "<ul class=\"nets\" id=\"pl\"><li class=\"status\">Tap Search to look for boxes on the network</li></ul>"
             "<form method=\"POST\" action=\"/copy-list\" accept-charset=\"UTF-8\" class=\"field\">"
             "<label for=\"peer\">Box address</label><div class=\"row-inline\">");
  res->printf("<input class=\"input\" id=\"peer\" name=\"peer\" type=\"text\" value=\"%s\" maxlength=\"60\" placeholder=\"other-box.local\" required autocomplete=\"off\" spellcheck=\"false\">",
              htmlEscape(peer).c_str());
  res->print("<button class=\"btn secondary\" type=\"submit\">Load album</button></div>"
             "<p class=\"hint\">The other box needs its management token in place so its server runs. Its album token is not needed.</p></form></div>");

  // Card 2: the peer's album, the missing files and the transfer
  res->print("<div class=\"card\"><div class=\"card-head\"><h2 id=\"ct\">Album</h2><span class=\"hint mono\" id=\"cr\"></span></div>"
             "<div class=\"progress\" id=\"cp\" hidden><i id=\"cb\"></i></div>"
             "<p class=\"hint\" id=\"cs\">No album loaded yet.</p>"
             "<ul class=\"ulist\" id=\"cu\" hidden></ul>"
             "<div class=\"actions end\">"
             "<form method=\"POST\" action=\"/copy-cancel\"><button class=\"btn destructive\" type=\"submit\" id=\"cc\" hidden>Cancel</button></form>"
             "<form method=\"POST\" action=\"/copy-start\"><button class=\"btn\" type=\"submit\" id=\"cg\" hidden>Copy missing files</button></form>"
             "</div></div>");

  pageFoot(res);
  request->send(res);
}

void registerSyncRoutes(AsyncWebServer &server) {
  server.on("/peer-info", HTTP_GET, peerInfo);
  server.on("/peer-album", HTTP_GET, peerAlbum);
  server.on("/peer-file", HTTP_GET, peerFile);

  server.on("/copy", HTTP_GET, copyPage);
  server.on("/copy-status", HTTP_GET, copyStatusJson);
  server.on("/copy-scan", HTTP_GET, copyScanJson);
  server.on("/copy-list", HTTP_POST, copyListPost);
  server.on("/copy-start", HTTP_POST, copyStartPost);
  server.on("/copy-cancel", HTTP_POST, copyCancelPost);
}

#endif
