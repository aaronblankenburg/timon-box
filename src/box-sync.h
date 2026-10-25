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
#define SYNC_PARALLEL 2  // files in flight; measured 1.36x over a single connection

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

// One line "album\t<display name>", then one "<size>,<duration ms>\t<name>" per track.
// Line based on purpose: the fetching box parses this without a JSON parser. Older firmware reads
// the size with strtoul and stops at the comma, so it still understands the lines.
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

  auto page = std::make_shared<PageBuffer>();
  Print *res = page.get();
  res->printf("album\t%s\n", folderDisplayName(folder, tag).c_str());
  int count = 0;
  PodcastState podcast;  // an episode still downloading is not offered: it would arrive cut off
  readPodcastState(folder, podcast);
  String partial = podcast.complete ? "" : podcast.file;
  File dir = SD.open(folder);
  while (File entry = dir.openNextFile()) {
    if (!entry.isDirectory() && String(entry.name()).endsWith(".mp3") && partial != entry.name() && count < SYNC_MAX_FILES) {
      esp_task_wdt_reset();  // up to 200 header reads
      res->printf("%u,%u\t%s\n", (unsigned)entry.size(), (unsigned)mp3DurationMs(entry), entry.name());
      count++;
    }
    entry.close();
  }
  dir.close();
  filesUnlock();

  Serial.printf("[peer] album %s for %s: %d files\n", folder.c_str(), tag.c_str(), count);
  sendBuffered(request, page, "text/plain");
}

// The open File has to survive across filler calls, so it lives on the heap and
// is released in onDisconnect, which also fires after a completed response.
// The read-ahead buffer matters: the filler is asked for about one TCP segment
// at a time, and reading 1.4 kB from the card per call costs a good quarter of
// the throughput (measured: 213 kB/s from the card against 296 kB/s from flash).
struct PeerFileCtx {
  File file;
  size_t size;
  uint8_t *buffer = nullptr;  // borrowed from the upload pool, returned on disconnect
  size_t fill = 0;
  size_t taken = 0;
};

// Only the AsyncTCP task writes these, the loop task reads them for the LED.
volatile int peerTransfers = 0;
volatile unsigned long peerLastChunk = 0;

// True while tracks are going out to the other box. The tail keeps the LED
// pulsing across the gap between two files instead of blinking off.
bool peerTransferActive() {
  return peerTransfers > 0 || millis() - peerLastChunk < 500;
}

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
  auto ctx = std::make_shared<PeerFileCtx>();  // not brace-initialised: the struct has default members
  ctx->file = file;
  ctx->size = size;
  // Idle unless an upload runs at the same time; without it we read per segment.
  if (uploadFreeBufs) xQueueReceive(uploadFreeBufs, &ctx->buffer, 0);
  peerTransfers++;
  request->onDisconnect([ctx]() {
    ctx->file.close();
    if (ctx->buffer) {
      xQueueSend(uploadFreeBufs, &ctx->buffer, 0);
      ctx->buffer = nullptr;
    }
    peerTransfers--;
  });

  // Own filler instead of request->send(SD, ...): AsyncFileResponse reads the
  // card unlocked from the AsyncTCP task, and in 3.7.10 it also drops the
  // content type we pass.
  AsyncWebServerResponse *response = request->beginResponse("audio/mpeg", size, [ctx](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    if (index >= ctx->size) return 0;
    if (!filesLock(50)) return RESPONSE_TRY_AGAIN;  // another task owns the card right now
    size_t read = 0;
    if (ctx->buffer) {
      if (ctx->taken == ctx->fill) {  // refill in one big read instead of one per segment
        int got = ctx->file.read(ctx->buffer, UPLOAD_BUF_SIZE);
        ctx->fill = got > 0 ? (size_t)got : 0;
        ctx->taken = 0;
      }
      read = std::min(maxLen, ctx->fill - ctx->taken);
      if (read > 0) {
        memcpy(buffer, ctx->buffer + ctx->taken, read);
        ctx->taken += read;
      }
    } else {
      int got = ctx->file.read(buffer, std::min(maxLen, ctx->size - index));
      read = got > 0 ? (size_t)got : 0;
    }
    filesUnlock();
    peerLastChunk = millis();
    return read;
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
  uint32_t loaded;  // bytes fetched so far, for the row in the UI
  uint8_t status;
  uint32_t ms;  // duration, 0 = the other box did not say
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
    int comma = head.indexOf(',');
    uint32_t ms = comma > 0 ? strtoul(head.c_str() + comma + 1, nullptr, 10) : 0;  // older firmware sends none
    files.push_back({rest, size, 0, (uint8_t)(present ? SF_PRESENT : SF_PENDING), ms});
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

// Two files are fetched at once from one task: each connection is capped by the
// peer's TCP window, and a second one lifted the measured throughput by ~1.36x.
// One task rather than two keeps the SD writes serialised and costs no second
// stack; the sockets are simply polled in turn.
struct SyncSlot {
  bool active = false;
  int index = -1;
  String name;
  String tmpPath;
  uint32_t expected = 0;
  uint32_t got = 0;
  WiFiClient client;
  HTTPClient http;
  WiFiClient *stream = nullptr;
  File out;
  uint8_t *buffer = nullptr;
  size_t bufferSize = 0;
  size_t fill = 0;
  unsigned long startedAt = 0;
  unsigned long lastData = 0;
  unsigned long sdMs = 0;
};

static SyncSlot syncSlots[SYNC_PARALLEL];  // file scope: keeps them off the worker's stack

static void syncSetFileLoaded(size_t index, uint32_t loaded) {
  if (!syncLock(200)) return;
  if (index < syncFiles.size()) syncFiles[index].loaded = loaded;
  syncUnlock();
}

// Writes what the slot has collected so far. Caller reports the failure.
static bool syncSlotFlush(SyncSlot &slot) {
  if (!slot.fill) return true;
  unsigned long writeStart = millis();
  bool written = false;
  if (filesLock(10000)) {
    written = slot.out.write(slot.buffer, slot.fill) == slot.fill;
    filesUnlock();
  }
  slot.sdMs += millis() - writeStart;
  slot.fill = 0;
  return written;
}

static bool syncSlotStart(SyncSlot &slot, const String &addr, const String &tag, int index, const String &name, uint32_t size) {
  slot.index = index;
  slot.name = name;
  slot.expected = 0;
  slot.got = 0;
  slot.fill = 0;
  slot.sdMs = 0;
  slot.startedAt = millis();
  slot.lastData = millis();

  slot.http.setConnectTimeout(SYNC_CONNECT_TIMEOUT_MS);
  slot.http.setTimeout(SYNC_READ_TIMEOUT_MS);
  String url = "http://" + addr + "/peer-file?tag=" + tag + "&name=" + urlEncode(name);
  if (!slot.http.begin(slot.client, url)) {
    Serial.printf("[sync] %s: could not open the connection\n", name.c_str());
    return false;
  }
  int code = slot.http.GET();
  if (code != 200) {
    Serial.printf("[sync] %s: HTTP %d\n", name.c_str(), code);
    slot.http.end();
    return false;
  }
  slot.expected = slot.http.getSize() > 0 ? (uint32_t)slot.http.getSize() : size;
  slot.stream = slot.http.getStreamPtr();

  if (filesLock(10000)) {
    if (SD.exists(slot.tmpPath)) SD.remove(slot.tmpPath);  // leftover from an aborted transfer
    slot.out = SD.open(slot.tmpPath, FILE_WRITE);
    filesUnlock();
  }
  if (!slot.out) {
    Serial.printf("[sync] %s: could not open the temp file\n", name.c_str());
    slot.http.end();
    return false;
  }
  slot.active = true;
  syncSetFileStatus(index, SF_COPYING);
  return true;
}

// Closes the slot down and, when the transfer was complete, moves the temp file
// into the album folder, overwriting a track of the same name.
static bool syncSlotFinish(SyncSlot &slot, bool ok) {
  if (ok) ok = syncSlotFlush(slot);
  slot.http.end();

  if (filesLock(10000)) {
    slot.out.close();
    if (ok) {
      String target = webServerFolderPath + "/" + slot.name;
      if (SD.exists(target)) SD.remove(target);
      ok = SD.rename(slot.tmpPath, target);
      if (ok) {
        filesInDir.erase(std::remove(filesInDir.begin(), filesInDir.end(), slot.name), filesInDir.end());
        filesInDir.push_back(slot.name);
      }
    }
    if (!ok && SD.exists(slot.tmpPath)) SD.remove(slot.tmpPath);
    filesUnlock();
  }

  unsigned long ms = millis() - slot.startedAt;
  Serial.printf("[sync] %s %s, %u bytes in %lu ms (%.2f MB/s), SD busy %lu ms, heap %u\n",
                slot.name.c_str(), ok ? "copied" : "failed", (unsigned)slot.got, ms,
                ms ? slot.got / 1048.576 / ms : 0.0, slot.sdMs, ESP.getFreeHeap());

  syncSetFileStatus(slot.index, ok ? SF_DONE : SF_FAILED);
  if (ok) syncBytesDone += slot.got;
  slot.active = false;
  slot.stream = nullptr;
  return ok;
}

// Moves one slot along. Sets progressed when bytes were handled, done when the
// slot finished (ok then says whether the file made it).
static void syncSlotPump(SyncSlot &slot, bool &progressed, bool &done, bool &ok) {
  done = false;
  ok = false;
  size_t available = slot.stream->available();
  if (!available) {
    if (slot.got >= slot.expected) {
      done = true;
      ok = syncSlotFinish(slot, true);
      return;
    }
    if (!slot.stream->connected()) {
      Serial.printf("[sync] %s: connection closed after %u of %u bytes\n", slot.name.c_str(), (unsigned)slot.got, (unsigned)slot.expected);
      done = true;
      ok = syncSlotFinish(slot, false);
      return;
    }
    if (millis() - slot.lastData > SYNC_STALL_TIMEOUT_MS) {
      Serial.printf("[sync] %s: stalled after %u bytes\n", slot.name.c_str(), (unsigned)slot.got);
      done = true;
      ok = syncSlotFinish(slot, false);
    }
    return;
  }

  int read = slot.stream->readBytes(slot.buffer + slot.fill, std::min(available, slot.bufferSize - slot.fill));
  if (read <= 0) return;
  progressed = true;
  slot.lastData = millis();
  slot.fill += read;
  slot.got += read;
  syncSetFileLoaded(slot.index, slot.got);
  if (slot.fill == slot.bufferSize && !syncSlotFlush(slot)) {
    Serial.printf("[sync] %s: writing to the SD card failed\n", slot.name.c_str());
    done = true;
    ok = syncSlotFinish(slot, false);
    return;
  }
  if (slot.got >= slot.expected) {
    done = true;
    ok = syncSlotFinish(slot, true);
  }
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

  // Buffers come from the upload pool, which is idle unless an upload runs at
  // the same time. However many we get is how many files we fetch at once; the
  // last resort is a small static buffer and a single stream.
  static uint8_t fallbackBuffer[1436];
  int slots = 0;
  for (int i = 0; i < SYNC_PARALLEL; i++) {
    SyncSlot &slot = syncSlots[i];
    slot.buffer = nullptr;
    slot.active = false;
    slot.tmpPath = String(SYNC_TEMP_FILE) + String(i);
    if (uploadFreeBufs && xQueueReceive(uploadFreeBufs, &slot.buffer, pdMS_TO_TICKS(100)) == pdTRUE) {
      slot.bufferSize = UPLOAD_BUF_SIZE;
      slots++;
    } else if (!slots) {  // at least one stream, even without a pool buffer
      slot.buffer = fallbackBuffer;
      slot.bufferSize = sizeof(fallbackBuffer);
      slots++;
    }
  }

  Serial.printf("[sync] copying %d files (%s) from %s over %d connection%s, heap %u, largest block %u\n",
                pending, formatBytes(needed).c_str(), addr.c_str(), slots, slots == 1 ? "" : "s",
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  int copied = 0, failed = 0;
  size_t next = 0;
  int active = 0;
  while (true) {
    // fill free slots with the next file that still needs copying
    for (int i = 0; i < slots && !syncCancel; i++) {
      SyncSlot &slot = syncSlots[i];
      if (slot.active) continue;
      while (next < work.size() && work[next].status != SF_PENDING && work[next].status != SF_FAILED) next++;
      if (next >= work.size()) break;
      size_t index = next++;
      if (!syncSlotStart(slot, addr, tag, index, work[index].name, work[index].size)) {
        syncSetFileStatus(index, SF_FAILED);
        failed++;
        continue;
      }
      active++;
    }
    if (!active) break;

    bool progressed = false;
    uint32_t inFlight = 0;
    for (int i = 0; i < slots; i++) {
      SyncSlot &slot = syncSlots[i];
      if (!slot.active) continue;
      if (syncCancel) {
        syncSlotFinish(slot, false);
        active--;
        failed++;
        continue;
      }
      bool done = false, ok = false;
      syncSlotPump(slot, progressed, done, ok);
      if (done) {
        active--;
        if (ok) copied++;
        else failed++;
      } else {
        inFlight += slot.got;
      }
    }
    syncBytesCur = inFlight;
    if (syncCancel && !active) break;
    if (!progressed) vTaskDelay(pdMS_TO_TICKS(2));
  }
  syncBytesCur = 0;
  syncCurrent = -1;

  for (int i = 0; i < slots; i++) {  // give the borrowed buffers back
    SyncSlot &slot = syncSlots[i];
    if (slot.buffer && slot.buffer != fallbackBuffer) xQueueSend(uploadFreeBufs, &slot.buffer, 0);
    slot.buffer = nullptr;
    slot.name = "";
  }

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
  auto page = std::make_shared<PageBuffer>();
  Print *res = page.get();
  res->printf("{\"state\":\"%s\",\"station\":%s,\"bytes\":%u,\"bytesTotal\":%u,\"heap\":%u",
              syncStateName(state),
              netMode == NET_STATION && WiFi.status() == WL_CONNECTED ? "true" : "false",
              (unsigned)(syncBytesDone + syncBytesCur), (unsigned)syncBytesTotal, ESP.getFreeHeap());

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
      res->printf("{\"name\":\"%s\",\"size\":%u,\"ms\":%u,\"loaded\":%u,\"status\":\"%s\"}",
                  jsonEscape(file.name).c_str(), (unsigned)file.size, (unsigned)file.ms, (unsigned)file.loaded,
                  syncFileStatusName(file.status));
    }
    res->print("]");
    syncUnlock();
  } else {
    res->print(",\"peer\":\"\",\"album\":\"\",\"error\":\"\",\"files\":[]");
  }
  res->printf(",\"total\":%d,\"missing\":%d,\"done\":%d,\"failed\":%d}", total, missing, done, failed);
  sendBuffered(request, page, "application/json", true);
}

void copyScanJson(AsyncWebServerRequest *request) {
  if (!syncBusy() && (request->hasParam("rescan") || (!syncScanDone && !syncScanning))) {
    syncQueue(SYNC_OP_SCAN);
  }
  const char *state = syncBusy() ? "busy" : (syncScanning || !syncScanDone) ? "scanning" : "done";

  auto page = std::make_shared<PageBuffer>();
  Print *res = page.get();
  res->printf("{\"state\":\"%s\",\"boxes\":[", state);
  if (syncLock(500)) {
    for (size_t i = 0; i < syncPeers.size(); i++) {
      if (i) res->print(',');
      res->printf("{\"name\":\"%s\",\"host\":\"%s\"}", jsonEscape(syncPeers[i].name).c_str(), jsonEscape(syncPeers[i].host).c_str());
    }
    syncUnlock();
  }
  res->print("]}");
  sendBuffered(request, page, "application/json", true);
}

void copyListPost(AsyncWebServerRequest *request) {
  String peer = request->hasParam("peer", true) ? request->getParam("peer", true)->value() : "";
  peer.trim();
  if (peer.isEmpty() || peer.length() > 60) {
    request->redirect("/copy?err=1");
    return;
  }
  if (syncBusy() || episodesBusy()) {
    request->redirect("/copy?err=2");
    return;
  }
  syncQueue(SYNC_OP_LIST, peer);  // the worker stores it in NVS once it answers
  request->redirect("/copy");
}

void copyStartPost(AsyncWebServerRequest *request) {
  if (syncBusy() || episodesBusy() || syncState == SYNC_IDLE) {
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

  auto page = std::make_shared<PageBuffer>();
  Print *res = page.get();
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

  // Card 2: the peer's album and what is missing here
  res->print("<div class=\"card\"><div class=\"card-head\"><h2 id=\"ct\">Album</h2><span class=\"hint mono\" id=\"cr\"></span></div>"
             "<p class=\"hint\" id=\"cs\">No album loaded yet.</p>"
             "<div id=\"cslot\"><ul class=\"ulist\" id=\"cu\" hidden></ul></div>"
             "<div class=\"actions end\">"
             "<button class=\"btn secondary\" type=\"button\" id=\"cw\" hidden>Show progress</button>"
             "<form method=\"POST\" action=\"/copy-start\"><button class=\"btn\" type=\"submit\" id=\"cg\" hidden>Copy missing files</button></form>"
             "</div></div>");

  // Transfer dialog, same shape as the upload one; the file list moves in here
  res->print("<dialog class=\"modal\" id=\"cd\"><div class=\"card\">"
             "<div class=\"card-head\"><h2 id=\"cdt\">Copying</h2><span class=\"hint mono\" id=\"cdr\"></span></div>"
             "<div class=\"progress\"><i id=\"cdb\"></i></div>"
             "<p class=\"hint mono\" id=\"cds\"></p>"
             "<div id=\"cmslot\"></div>"
             "<p class=\"hint\" id=\"cdh\">The transfer runs on the box. You can close this page, it keeps going.</p>"
             "<div class=\"actions end\">"
             "<form method=\"POST\" action=\"/copy-cancel\"><button class=\"btn destructive\" type=\"submit\" id=\"cdc\">Cancel</button></form>"
             "<button class=\"btn secondary\" type=\"button\" id=\"cdx\" hidden>Close</button>"
             "<form method=\"POST\" action=\"/copy-start\"><button class=\"btn\" type=\"submit\" id=\"cdg\" hidden>Retry</button></form>"
             "<button class=\"btn\" type=\"button\" id=\"cdf\" hidden>Show files</button>"
             "</div></div></dialog>");

  pageFoot(res);
  sendBuffered(request, page, "text/html");
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
