#ifndef PODCAST_H
#define PODCAST_H

// Podcast albums: fetch the newest episode of a feed into the album folder while it plays.
//
// Everything network-related runs on one task on core 0, away from the audio loop on core 1.
// The episode is written to the card in 4 kB blocks under sdGate and flushed regularly; the
// player reads the growing file through gatedFS (gated-fs.h).

#include <HTTPClient.h>
#include <MD5Builder.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>

#include <memory>

#include "gated-fs.h"
#include "podcast-feed.h"
#include "wifi-setup.h"

#define PODCAST_USER_AGENT BOX_NAME " podcast player (ESP32)"
#define PODCAST_TASK_STACK 12288
#define PODCAST_WIFI_TIMEOUT_MS 10000
#define PODCAST_STALL_TIMEOUT_MS 20000
#define PODCAST_BLOCK 4096
#define PODCAST_FIRST_FLUSH (64 * 1024)  // playback can start once this much is on the card
#define PODCAST_FLUSH_EVERY (128 * 1024)
#define PODCAST_HUNGRY_FLUSH (16 * 1024)  // flush early by this much when a reader is waiting

volatile TaskHandle_t podcastTask = nullptr;
volatile bool podcastCancel = false;

bool podcastBusy() { return podcastTask != nullptr; }

void logHeap(const char *what) {
  Serial.printf("[heap] %s: free %u, min %u, largest block %u\n", what, ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                ESP.getMaxAllocHeap());
}

// ---------------------------------------------------------------------------
// HTTP
// ---------------------------------------------------------------------------

// One GET with redirects followed by hand: HTTPClient only follows them within the same
// protocol, and podcast tracking prefixes hop between http and https. HTTP/1.0 keeps servers
// from answering chunked; a server that does it anyway is decoded here.
class HttpStream {
 public:
  int status = 0;
  uint32_t length = SIZE_UNKNOWN;  // Content-Length of the body
  bool hasRange = false;           // a valid Content-Range came back
  uint32_t rangeStart = 0, rangeEnd = 0, rangeTotal = SIZE_UNKNOWN;
  String url;  // where the body comes from, after redirects
  String error;

  ~HttpStream() { close(); }

  bool open(const String &startUrl, uint32_t from = SIZE_UNKNOWN, uint32_t to = SIZE_UNKNOWN,
            int32_t connectTimeoutMs = 5000) {
    close();
    url = startUrl;
    for (int hop = 0; hop < 10; hop++) {
      bool tls = url.startsWith("https://");
      if (!tls && !url.startsWith("http://")) {
        error = "not an http(s) URL";
        return false;
      }
      WiFiClient *client = &_plain;
      if (tls) {
        if (!_secure) _secure.reset(new WiFiClientSecure());
        _secure->setInsecure();  // see plainHttpVariant() in podcast-feed.h
        _secure->setHandshakeTimeout(15);
        client = _secure.get();
      }
      static const char *keys[] = {"Content-Range", "Transfer-Encoding", "Location"};
      _http.setReuse(false);
      _http.useHTTP10(true);
      _http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
      _http.setConnectTimeout(connectTimeoutMs);
      _http.setTimeout(10000);
      _http.setUserAgent(PODCAST_USER_AGENT);
      _http.collectHeaders(keys, 3);
      if (!_http.begin(*client, url)) {
        error = "bad URL";
        return false;
      }
      if (from != SIZE_UNKNOWN) {
        _http.addHeader("Range", "bytes=" + String(from) + "-" + (to != SIZE_UNKNOWN ? String(to) : String("")));
      }
      status = _http.GET();
      if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
        String location = _http.header("Location");
        _http.end();
        if (location.isEmpty()) {
          error = "redirect without Location";
          return false;
        }
        url = String(resolveUrl(url.c_str(), location.c_str()).c_str());
        continue;
      }
      break;
    }
    if (status <= 0) {
      error = HTTPClient::errorToString(status);
      _http.end();
      return false;
    }
    if (status != 200 && status != 206) {
      error = "HTTP " + String(status);
      _http.end();
      return false;
    }
    int size = _http.getSize();
    length = size >= 0 ? (uint32_t)size : SIZE_UNKNOWN;
    hasRange = parseContentRange(_http.header("Content-Range").c_str(), rangeStart, rangeEnd, rangeTotal);
    _chunked = _http.header("Transfer-Encoding").equalsIgnoreCase("chunked");
    _left = _chunked ? SIZE_UNKNOWN : length;
    _chunkLeft = 0;
    _chunkEnd = false;
    _stream = _http.getStreamPtr();
    if (!_stream) {
      error = "no stream";
      _http.end();
      return false;
    }
    return true;
  }

  // > 0: bytes read, 0: nothing available right now, -1: the body ended (or the connection did).
  int read(uint8_t *buf, size_t len) {
    if (!_stream) return -1;
    if (_chunked) {
      if (_chunkEnd) return -1;
      if (_chunkLeft == 0 && !readChunkHeader()) return _chunkEnd ? -1 : 0;
      if (len > _chunkLeft) len = _chunkLeft;
    }
    if (_left == 0) return -1;
    if (_left != SIZE_UNKNOWN && len > _left) len = _left;
    int avail = _stream->available();
    if (avail <= 0) return _stream->connected() ? 0 : -1;
    if ((size_t)avail < len) len = avail;
    int n = _stream->read(buf, len);
    if (n <= 0) return 0;
    if (_left != SIZE_UNKNOWN) _left -= n;
    if (_chunked) _chunkLeft -= n;
    return n;
  }

  void close() {
    _stream = nullptr;
    _http.end();  // stops the client and with it the TLS session
    _secure.reset();
  }

 private:
  WiFiClient _plain;
  std::unique_ptr<WiFiClientSecure> _secure;  // only for https: its constructor allocates 2.2 kB
  HTTPClient _http;
  WiFiClient *_stream = nullptr;
  uint32_t _left = SIZE_UNKNOWN;
  bool _chunked = false;
  uint32_t _chunkLeft = 0;
  bool _chunkEnd = false;

  // "<hex>[;extension]\r\n" after the CRLF that ends the previous chunk. Rare, so it simply waits.
  bool readChunkHeader() {
    char line[24];
    size_t used = 0;
    unsigned long start = millis();
    while (millis() - start < 10000) {
      int c = _stream->read();
      if (c < 0) {
        if (!_stream->connected()) break;
        vTaskDelay(pdMS_TO_TICKS(2));
        continue;
      }
      if (c == '\n') {
        line[used] = 0;
        if (used == 0 || (used == 1 && line[0] == '\r')) continue;  // CRLF after chunk data
        _chunkLeft = strtoul(line, nullptr, 16);
        if (_chunkLeft == 0) break;  // last chunk
        return true;
      }
      if (used < sizeof(line) - 1) line[used++] = (char)c;
    }
    _chunkEnd = true;
    return false;
  }
};

// ---------------------------------------------------------------------------
// Wi-Fi
// ---------------------------------------------------------------------------

bool podcastWifiUp(String &error) {
  String ssid, pass;
  if (!readSavedCredentials(ssid, pass)) {
    error = "no saved Wi-Fi network";
    return false;
  }
  WiFi.persistent(false);  // never write the credentials to flash: that stalls the audio
  WiFi.setAutoReconnect(false);
  WiFi.setHostname(boxHostname());
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // modem sleep halves the download rate
  WiFi.begin(ssid.c_str(), pass.length() ? pass.c_str() : nullptr);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (podcastCancel || millis() - start > PODCAST_WIFI_TIMEOUT_MS) {
      error = podcastCancel ? "cancelled" : "could not join \"" + ssid + "\"";
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  Serial.printf("[podcast] joined \"%s\" after %lu ms, rssi %d\n", ssid.c_str(), millis() - start, WiFi.RSSI());
  return true;
}

void podcastWifiDown() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

// ---------------------------------------------------------------------------
// Episode download
// ---------------------------------------------------------------------------

struct DownloadStats {
  uint32_t bytes = 0;  // received in this session
  unsigned long startedMs = 0;
  uint32_t maxWriteMs = 0, maxFlushMs = 0;
  uint32_t minFree = 0xFFFFFFFF, minBlock = 0xFFFFFFFF;
  unsigned long lastReport = 0;

  void sample() {
    uint32_t f = ESP.getFreeHeap(), b = ESP.getMaxAllocHeap();
    if (f < minFree) minFree = f;
    if (b < minBlock) minBlock = b;
  }
};

// What is already on the card for `path`.
static void localEpisodeState(const String &path, uint32_t &size, bool &startsWithId3) {
  size = 0;
  startsWithId3 = false;
  if (!sdGateTake()) return;
  File f = SD.open(path, FILE_READ);
  if (f) {
    size = f.size();
    uint8_t head[3];
    startsWithId3 = size >= 3 && f.read(head, 3) == 3 && memcmp(head, "ID3", 3) == 0;
    f.close();
  }
  sdGateGive();
}

// Reads exactly `len` bytes unless the body ends or stalls. Returns the count.
static size_t readFully(HttpStream &http, uint8_t *buf, size_t len) {
  size_t got = 0;
  unsigned long last = millis();
  while (got < len && !podcastCancel) {
    int n = http.read(buf + got, len - got);
    if (n < 0) break;
    if (n == 0) {
      if (millis() - last > PODCAST_STALL_TIMEOUT_MS) break;
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }
    got += n;
    last = millis();
  }
  return got;
}

// Downloads `url` into `path`, leaving out a leading ID3 tag (podcast covers reach 1 MB, and
// the audio library skips a tag only 256 bytes per loop) and continuing a partial file with a
// Range request. Progress is published through `growing`, so the file plays while it grows.
// One attempt; returns true when the file is complete.
static bool downloadEpisodeOnce(const String &url, const String &path, bool allowSkip, String &error,
                                DownloadStats &st, bool &retrySkipless) {
  retrySkipless = false;
  uint32_t local;
  bool localHasId3;
  localEpisodeState(path, local, localHasId3);

  // Probe: the first 10 bytes tell the ID3 size, Content-Range the total and range support.
  HttpStream probe;
  if (!probe.open(url, 0, 9)) {
    error = "episode: " + probe.error;
    return false;
  }
  uint8_t hdr[10];
  size_t hdrLen = readFully(probe, hdr, sizeof(hdr));
  bool ranges = probe.status == 206 && probe.hasRange;
  uint32_t total = ranges ? probe.rangeTotal : probe.length;
  // A file on the card that starts with a tag is a full copy (copied from the other box, or the
  // tag was kept); one without continues after the skipped tag.
  uint32_t skip = (allowSkip && !localHasId3) ? id3TagSize(hdr, hdrLen) : 0;
  if (total != SIZE_UNKNOWN && skip >= total) skip = 0;
  uint32_t expected = total != SIZE_UNKNOWN ? total - skip : SIZE_UNKNOWN;
  Serial.printf("[podcast] %s: %s, total %d, id3 %u, on card %u\n", path.c_str(),
                ranges ? "ranges ok" : "no ranges", total == SIZE_UNKNOWN ? -1 : (int)total, skip, local);

  if (expected != SIZE_UNKNOWN && local == expected) {
    growingDone(local);
    return true;  // already complete
  }
  if (!ranges || (expected != SIZE_UNKNOWN && local > expected)) local = 0;  // cannot continue: start over

  HttpStream body;
  HttpStream *in = &probe;
  uint32_t discard = 0;  // bytes at the start of `in` that do not go into the file
  uint8_t *pending = nullptr;
  size_t pendingLen = 0;
  if (ranges) {
    probe.close();
    uint32_t from = skip + local;
    // Prefer plain HTTP for the long transfer, but only if it serves the same file.
    String plain = String(plainHttpVariant(probe.url.c_str()).c_str());
    bool ok = plain != probe.url && body.open(plain, from, SIZE_UNKNOWN, 3000) && body.status == 206 &&
              body.hasRange && body.rangeStart == from && body.rangeTotal == total;
    if (!ok) ok = body.open(probe.url, from) && body.status == 206 && body.hasRange && body.rangeStart == from &&
                  body.rangeTotal == total;
    if (!ok) {
      error = "episode: " + (body.error.length() ? body.error : String("range request refused"));
      return false;
    }
    Serial.printf("[podcast] body from %s at %u\n", body.url.startsWith("https") ? "https" : "http", from);
    in = &body;
  } else {
    // The probe connection carries the whole file; its first bytes are already in hdr.
    discard = skip;
    if (hdrLen > discard) {
      pending = hdr + discard;
      pendingLen = hdrLen - discard;
      discard = 0;
    } else {
      discard -= hdrLen;
    }
  }

  uint8_t *block = (uint8_t *)malloc(PODCAST_BLOCK);
  if (!block) {
    error = "no memory for the download buffer";
    return false;
  }
  File out;
  if (sdGateTake()) {
    out = SD.open(path, local > 0 ? FILE_APPEND : FILE_WRITE);
    if (out) out.setBufferSize(512);  // whole 4 kB blocks bypass the stdio buffer anyway
    sdGateGive();
  }
  if (!out) {
    free(block);
    error = "cannot write " + path;
    return false;
  }
  growingStart(path.c_str(), local, expected == SIZE_UNKNOWN ? 0 : expected);

  uint32_t written = local, flushed = local;
  size_t fill = 0;
  bool checkedSync = local > 0 || skip == 0;
  bool ended = false, stalled = false;
  unsigned long lastData = millis();
  if (pendingLen) {
    memcpy(block, pending, pendingLen);
    fill = pendingLen;
  }

  auto writeBlock = [&](size_t len) -> bool {
    unsigned long t0 = millis();
    if (!sdGateTake()) return false;
    bool ok = out.write(block, len) == len;
    sdGateGive();
    uint32_t ms = millis() - t0;
    if (ms > st.maxWriteMs) st.maxWriteMs = ms;
    if (ok) written += len;
    return ok;
  };
  auto flushOut = [&]() {
    unsigned long t0 = millis();
    if (!sdGateTake()) return;
    out.flush();
    sdGateGive();
    uint32_t ms = millis() - t0;
    if (ms > st.maxFlushMs) st.maxFlushMs = ms;
    flushed = written;
    growingSynced(flushed);
  };

  bool writeFailed = false;
  while (!podcastCancel) {
    int n = in->read(block + fill, PODCAST_BLOCK - fill);
    if (n < 0) {
      ended = true;
      break;
    }
    if (n == 0) {
      if (millis() - lastData > PODCAST_STALL_TIMEOUT_MS) {
        stalled = true;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }
    lastData = millis();
    st.bytes += n;
    if (discard) {  // still inside the ID3 tag of a server without ranges
      size_t d = (size_t)n < discard ? n : discard;
      memmove(block + fill, block + fill + d, n - d);
      n -= d;
      discard -= d;
    }
    fill += n;
    if (!checkedSync && fill >= 2) {
      checkedSync = true;
      if (!isMp3FrameSync(block[0], block[1])) {  // something else follows the tag: keep it all
        retrySkipless = true;
        error = "no MP3 frame after the ID3 tag";
        break;
      }
    }
    if (fill == PODCAST_BLOCK) {
      if (!writeBlock(fill)) {
        writeFailed = true;
        break;
      }
      fill = 0;
      uint32_t since = written - flushed;
      if ((flushed == local && written - local >= PODCAST_FIRST_FLUSH) || since >= PODCAST_FLUSH_EVERY ||
          (since >= PODCAST_HUNGRY_FLUSH && growingHungry())) {
        flushOut();
      }
    }
    if (millis() - st.lastReport >= 2000) {
      st.lastReport = millis();
      st.sample();
      unsigned long secs = (millis() - st.startedMs) / 1000;
      Serial.printf("[podcast] %u kB on card, %lu kB/s, heap %u (min %u), largest %u, write max %u ms, flush max %u ms\n",
                    written / 1024, secs ? st.bytes / 1024 / secs : 0, ESP.getFreeHeap(), st.minFree,
                    ESP.getMaxAllocHeap(), st.maxWriteMs, st.maxFlushMs);
    }
  }
  if (fill && !retrySkipless && !writeFailed) writeBlock(fill);
  if (sdGateTake()) {
    out.close();
    sdGateGive();
  }
  free(block);

  if (retrySkipless) {
    if (sdGateTake()) {
      SD.remove(path);
      sdGateGive();
    }
    growingDone(0);
    return false;
  }
  bool complete = expected != SIZE_UNKNOWN ? written == expected : ended && !stalled && !writeFailed;
  if (complete) {
    growingDone(written);
    return true;
  }
  growingSynced(written);
  if (writeFailed) error = "writing to the SD card failed";
  else if (podcastCancel) error = "cancelled";
  else if (stalled) error = "download stalled";
  else error = "connection closed early";
  return false;
}

// With retries: a dropped connection continues where it stopped.
bool downloadEpisode(const String &url, const String &path, String &error) {
  static const uint16_t backoffMs[] = {2000, 5000, 10000};
  DownloadStats st;
  st.startedMs = millis();
  bool allowSkip = true;
  bool ok = false;
  for (int attempt = 0; attempt <= 3 && !podcastCancel; attempt++) {
    bool retrySkipless = false;
    ok = downloadEpisodeOnce(url, path, allowSkip, error, st, retrySkipless);
    if (ok) break;
    if (retrySkipless) {
      allowSkip = false;
      attempt--;  // not a network failure
      continue;
    }
    Serial.printf("[podcast] attempt %d: %s\n", attempt + 1, error.c_str());
    if (attempt < 3 && !podcastCancel) vTaskDelay(pdMS_TO_TICKS(backoffMs[attempt]));
  }
  st.sample();
  unsigned long secs = (millis() - st.startedMs) / 1000;
  Serial.printf("[podcast] %s %s: %u kB in %lu s, min heap %u, min largest %u, write max %u ms, flush max %u ms\n",
                path.c_str(), ok ? "complete" : "incomplete", st.bytes / 1024, secs, st.minFree, st.minBlock,
                st.maxWriteMs, st.maxFlushMs);
  if (!ok) {
    uint32_t synced, expected;
    bool done;
    if (growingGet(path.c_str(), synced, expected, done)) growingDone(synced);  // readers end cleanly
  }
  return ok;
}

// ---------------------------------------------------------------------------
// Serial test commands (dl:<url>, md5:<path>)
// ---------------------------------------------------------------------------

struct PodcastDebugJob {
  String url;
  String path;
};

static void podcastDebugTask(void *arg) {
  std::unique_ptr<PodcastDebugJob> job((PodcastDebugJob *)arg);
  logHeap("download task start");
  String error;
  if (podcastWifiUp(error)) {
    logHeap("wifi up");
    if (!downloadEpisode(job->url, job->path, error)) Serial.printf("[podcast] failed: %s\n", error.c_str());
    podcastWifiDown();
    logHeap("wifi down");
  } else {
    Serial.printf("[podcast] no network: %s\n", error.c_str());
  }
  podcastTask = nullptr;
  vTaskDelete(NULL);
}

bool podcastDebugDownload(const String &url, const String &path) {
  if (podcastTask) return false;
  podcastCancel = false;
  PodcastDebugJob *job = new PodcastDebugJob{url, path};
  TaskHandle_t handle = nullptr;
  if (xTaskCreatePinnedToCore(podcastDebugTask, "podcast", PODCAST_TASK_STACK, job, 1, &handle, 0) != pdPASS) {
    delete job;
    return false;
  }
  podcastTask = handle;
  return true;
}

// MD5 of a file on the card, to compare a download with `curl ... | md5sum`.
String fileMd5(const String &path, uint32_t &size) {
  size = 0;
  MD5Builder md5;
  md5.begin();
  uint8_t *buf = (uint8_t *)malloc(PODCAST_BLOCK);
  if (!buf) return "no memory";
  if (!sdGateTake()) {
    free(buf);
    return "card busy";
  }
  File f = SD.open(path, FILE_READ);
  if (!f) {
    sdGateGive();
    free(buf);
    return "no such file";
  }
  int n;
  while ((n = f.read(buf, PODCAST_BLOCK)) > 0) {
    md5.add(buf, n);
    size += n;
    esp_task_wdt_reset();
  }
  f.close();
  sdGateGive();
  free(buf);
  md5.calculate();
  return md5.toString();
}

#endif
