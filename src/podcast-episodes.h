#ifndef PODCAST_EPISODES_H
#define PODCAST_EPISODES_H

// Picking single podcast episodes on the Podcast page and loading them onto the token.
//
// Most feeds send no CORS headers, so the page cannot read them itself; the box does. A worker
// task reads the feed once and writes every MP3 episode as one line into EPISODES_LIST_FILE, with
// an index sorted newest first in RAM. The page shows that list a page at a time and marks what is
// already in the album folder. Picked episodes are loaded one after another by the podcast
// downloader (redirects, https, Range, ID3 skipping, retries, see podcast.h) into
// EPISODES_TEMP_FILE and then renamed into the album folder under the name the box gives its own
// downloads, so they sort by date with them. They are listed in podcast_kept.txt, which the
// cleanup of a podcast album leaves alone.
//
// Like the Copy page, the job runs on the box and the page only polls: a reload or a closed
// browser does not stop it. The worker writes through FATFS directly (see podcast.h); VFS calls
// (deleteIndexFiles) only under filesLock, like everything else in management mode.
//
// Included from management-server.h after box-sync.h: it uses filesLock, filesInDir,
// webServerFolderPath, deleteIndexFiles, syncBusy and peerTransferActive.

#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>

#include "podcast.h"
#include "web-ui.h"
#include "wifi-setup.h"

#define EPISODES_LIST_FILE "/temp_episodes.txt"
#define EPISODES_TEMP_FILE "/temp_episode"
#define EPISODES_MAX 1000                // newest episodes the list keeps: 8 kB of index
#define EPISODES_PAGE_MAX 50
#define EPISODES_JOBS_MAX 100            // episodes per download job
#define EPISODES_TASK_STACK 12288        // as the podcast task: the same HTTP and download code
#define EPISODES_MIN_BLOCK (40 * 1024)   // largest free heap block needed before a TLS connection

enum EpisodesListState : uint8_t { EL_NONE, EL_LISTING, EL_LISTED, EL_FAILED };
enum EpisodesJobState : uint8_t { EJ_NONE, EJ_RUNNING, EJ_DONE, EJ_CANCELLED };
enum EpisodeJobStatus : uint8_t { EJS_WAITING, EJS_LOADING, EJS_DONE, EJS_FAILED, EJS_PRESENT, EJS_CANCELLED };

struct EpisodeIndexEntry {
  uint32_t date;    // seconds since 1970, 0 = the feed gave none
  uint32_t offset;  // where the episode's line starts in EPISODES_LIST_FILE
};

// One line of EPISODES_LIST_FILE: date \t when \t size \t seconds \t file \t url \t title
struct EpisodeLine {
  uint32_t date = 0, size = 0, seconds = 0;
  String when, file, url, title;
};

struct EpisodeJob {
  int id = -1;  // position in the newest-first list
  String title, file, url;
  uint32_t ms = 0;    // duration from the feed
  uint32_t size = 0;  // what the feed says until the file is there, then the real size
  uint8_t status = EJS_WAITING;
  String error;
};

SemaphoreHandle_t episodesMutex = nullptr;  // guards the state below that the web handlers read
std::atomic<bool> episodesRunning{false};   // the worker exists; set before it starts
volatile EpisodesListState episodesListState = EL_NONE;
volatile EpisodesJobState episodesJobState = EJ_NONE;
volatile int episodesCurrent = -1;  // job being loaded
String episodesUrl, episodesTitle, episodesError, episodesFolder;
std::vector<EpisodeIndexEntry> episodesIndex;
std::vector<EpisodeJob> episodesJobs;
int episodesSkipped = 0;  // episodes in another format than MP3

bool episodesBusy() { return episodesRunning; }

static bool episodesLock(uint32_t waitMs = 500) {
  return episodesMutex && xSemaphoreTake(episodesMutex, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}
static void episodesUnlock() { xSemaphoreGive(episodesMutex); }

// Tabs and line breaks would break the line format.
static String oneField(const std::string &in) {
  String out;
  out.reserve(in.size());
  for (char c : in) out += (c == '\t' || c == '\n' || c == '\r') ? ' ' : c;
  out.trim();
  return out;
}

// Reads the lines at `offsets` from EPISODES_LIST_FILE.
static std::vector<EpisodeLine> readEpisodeLines(const std::vector<uint32_t> &offsets) {
  std::vector<EpisodeLine> lines(offsets.size());
  FIL *f = new (std::nothrow) FIL;
  char *buf = (char *)malloc(2048);  // a line: two dates, a size, a file name, a URL and a title
  if (f && buf && sdGateTake()) {
    if (f_open(f, fatPath(EPISODES_LIST_FILE).c_str(), FA_READ) == FR_OK) {
      for (size_t i = 0; i < offsets.size(); i++) {
        UINT got = 0;
        if (f_lseek(f, offsets[i]) != FR_OK || f_read(f, buf, 2047, &got) != FR_OK) continue;
        buf[got] = 0;
        char *end = strchr(buf, '\n');
        if (end) *end = 0;
        char *field[7] = {};
        int n = 0;
        for (char *p = buf; n < 7;) {
          field[n++] = p;
          char *tab = strchr(p, '\t');
          if (!tab) break;
          *tab = 0;
          p = tab + 1;
        }
        if (n < 7) continue;
        EpisodeLine &l = lines[i];
        l.date = strtoul(field[0], nullptr, 10);
        l.when = field[1];
        l.size = strtoul(field[2], nullptr, 10);
        l.seconds = strtoul(field[3], nullptr, 10);
        l.file = field[4];
        l.url = field[5];
        l.title = field[6];
      }
      f_close(f);
    }
    sdGateGive();
  }
  delete f;
  free(buf);
  return lines;
}

// ---------------------------------------------------------------------------
// Worker
// ---------------------------------------------------------------------------

// Reads the feed into EPISODES_LIST_FILE and the index.
static void episodesListFeed(const String &url) {
  FIL *out = new (std::nothrow) FIL;
  bool opened = false;
  if (out && sdGateTake()) {
    opened = f_open(out, fatPath(EPISODES_LIST_FILE).c_str(), FA_WRITE | FA_CREATE_ALWAYS) == FR_OK;
    sdGateGive();
  }
  if (!opened) {
    delete out;
    if (episodesLock(5000)) {
      episodesError = "cannot write the episode list to the SD card";
      episodesListState = EL_FAILED;
      episodesUnlock();
    }
    return;
  }

  std::vector<EpisodeIndexEntry> index;
  String pending;  // lines not written yet: the card gets 2 kB at a time
  uint32_t written = 0;
  bool writeOk = true;
  int skipped = 0;
  auto flush = [&]() {
    if (!pending.length()) return;
    if (sdGateTake()) {
      UINT put = 0;
      writeOk = f_write(out, pending.c_str(), pending.length(), &put) == FR_OK && put == pending.length() && writeOk;
      sdGateGive();
    } else {
      writeOk = false;
    }
    written += pending.length();
    pending = "";
  };

  FeedParser parser(1 << 30);  // every item, not only the first five
  parser.onItem = [&](const PodcastEpisode &e) {
    if (!e.isMp3()) {
      skipped++;
      return;
    }
    char when[20] = "";
    if (e.dated) {
      snprintf(when, sizeof(when), "%04d-%02d-%02d %02d:%02d", e.date.year, e.date.month, e.date.day, e.date.hour,
               e.date.minute);
    }
    uint32_t date = e.dated && e.date.utc > 0 ? (uint32_t)e.date.utc : 0;
    EpisodeIndexEntry entry = {date, written + pending.length()};
    pending += String(date) + "\t" + when + "\t" + String(e.length) + "\t" + String(e.seconds) + "\t" +
               episodeFileName(e).c_str() + "\t" + oneField(e.url) + "\t" + oneField(e.title) + "\n";
    if (index.size() < EPISODES_MAX) {
      index.push_back(entry);
    } else {  // a long feed: keep the newest
      auto oldest = std::min_element(index.begin(), index.end(),
                                     [](const EpisodeIndexEntry &a, const EpisodeIndexEntry &b) { return a.date < b.date; });
      if (entry.date > oldest->date) *oldest = entry;
    }
    if (pending.length() >= 2048) flush();
  };

  // TLS needs ~50 kB, a page being served at the same time more than what is left: lend it the
  // upload buffers meanwhile (management-server.h).
  bool lent = uploadPoolRelease();
  String error, serverDate;
  bool ok = fetchFeed(url, parser, error, serverDate);
  if (lent) uploadPoolRefill();
  flush();
  if (sdGateTake()) {
    if (f_close(out) != FR_OK) writeOk = false;
    sdGateGive();
  }
  delete out;
  if (ok && !writeOk) {
    ok = false;
    error = "writing the episode list to the SD card failed";
  }
  if (ok && index.empty()) {
    ok = false;
    error = skipped ? "the episodes of this podcast are not MP3 files, which the box cannot play" : "the feed lists no episodes";
  }
  std::stable_sort(index.begin(), index.end(),
                   [](const EpisodeIndexEntry &a, const EpisodeIndexEntry &b) { return a.date > b.date; });
  Serial.printf("[episodes] %s: %u episodes listed, %d not MP3%s%s\n", url.c_str(), (unsigned)index.size(), skipped,
                ok ? "" : ", failed: ", ok ? "" : error.c_str());

  if (episodesLock(5000)) {
    if (podcastCancel) {
      ok = false;
      error = "cancelled";
    }
    if (ok) {
      episodesIndex.swap(index);
      episodesTitle = oneField(parser.channelTitle);
      episodesSkipped = skipped;
    }
    episodesError = ok ? "" : error;
    episodesListState = ok ? EL_LISTED : EL_FAILED;
    episodesUnlock();
  }
}

// Moves the finished download into the album folder. The web handlers see the folder through
// filesInDir and the player through index.txt, so both are updated under filesLock.
static bool episodesMoveIntoAlbum(const String &folder, const String &file) {
  if (!filesLock(10000)) return false;
  bool ok = fatRename(EPISODES_TEMP_FILE, folder + "/" + file);
  if (ok) {
    if (webServerFolderPath == folder) {  // the album token may have been swapped meanwhile
      filesInDir.erase(std::remove(filesInDir.begin(), filesInDir.end(), file), filesInDir.end());
      filesInDir.push_back(file);
    }
    deleteIndexFiles(folder);
  }
  filesUnlock();
  if (ok && !addPodcastKeptName(folder, file)) Serial.printf("[episodes] could not add %s to the kept list\n", file.c_str());
  return ok;
}

static void episodesDownload() {
  String folder;
  if (episodesLock(5000)) {
    folder = episodesFolder;
    episodesUnlock();
  }
  for (size_t i = 0;; i++) {
    EpisodeJob job;
    if (!episodesLock(5000)) break;
    bool more = i < episodesJobs.size();
    if (more) {
      job = episodesJobs[i];
      episodesJobs[i].status = podcastCancel ? EJS_CANCELLED : EJS_LOADING;
      episodesCurrent = (int)i;
    }
    episodesUnlock();
    if (!more) break;
    if (podcastCancel) continue;  // marks the rest as cancelled

    String target = folder + "/" + job.file;
    uint32_t size = fileSizeOnCard(target);
    uint8_t status;
    String error;
    if (size > 0) {
      status = EJS_PRESENT;
    } else {
      Serial.printf("[episodes] loading %s\n", job.file.c_str());
      fatRemove(EPISODES_TEMP_FILE);
      uint32_t total;
      bool ok = downloadEpisode(job.url, EPISODES_TEMP_FILE, SIZE_UNKNOWN, error, total);
      if (ok) {
        size = fileSizeOnCard(EPISODES_TEMP_FILE);
        ok = episodesMoveIntoAlbum(folder, job.file);
        if (!ok) error = "could not move it into the album folder";
      }
      if (!ok) fatRemove(EPISODES_TEMP_FILE);
      status = ok ? EJS_DONE : podcastCancel ? EJS_CANCELLED : EJS_FAILED;
      if (!ok) size = job.size;
    }
    if (episodesLock(5000)) {
      episodesJobs[i].status = status;
      episodesJobs[i].size = size;
      episodesJobs[i].error = error;
      episodesUnlock();
    }
  }
  if (episodesLock(5000)) {
    episodesCurrent = -1;
    episodesJobState = podcastCancel ? EJ_CANCELLED : EJ_DONE;
    episodesUnlock();
  }
}

struct EpisodesOp {
  bool list;  // read the feed; else load episodesJobs
  String url;
};

static void episodesTaskMain(void *arg) {
  {  // a scope of its own: everything in it is freed before the task deletes itself
    std::unique_ptr<EpisodesOp> op((EpisodesOp *)arg);
    logHeap(op->list ? "episodes list start" : "episodes download start");
    if (op->list) {
      episodesListFeed(op->url);
    } else {
      episodesDownload();
    }
    logHeap("episodes task end");
  }
  episodesRunning = false;
  vTaskDelete(NULL);
}

static bool episodesStart(EpisodesOp *op) {
  podcastCancel = false;
  episodesRunning = true;
  if (xTaskCreatePinnedToCore(episodesTaskMain, "episodes", EPISODES_TASK_STACK, op, 1, nullptr, 0) != pdPASS) {
    episodesRunning = false;
    delete op;
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Web handlers
// ---------------------------------------------------------------------------

// Sends the reason and returns false when no feed or download may start now.
static bool episodesCanStart(AsyncWebServerRequest *request) {
  const char *why = nullptr;
  if (episodesBusy()) why = "The box is already reading a feed or loading episodes.";
  else if (syncBusy() || peerTransferActive()) why = "A copy between the boxes is running. Try again when it is done.";
  else if (uploadsActive > 0) why = "An upload is running. Try again when it is done.";
  else if (netMode != NET_STATION || WiFi.status() != WL_CONNECTED) why = "This needs the home network; the box is running its own hotspot.";
  if (why) {
    request->send(409, "text/plain", why);
    return false;
  }
  if (ESP.getMaxAllocHeap() < EPISODES_MIN_BLOCK) {
    request->send(503, "text/plain", "Not enough memory on the box right now. Try again in a moment.");
    return false;
  }
  return true;
}

static bool validFeedUrl(const String &url) {
  return (url.startsWith("http://") || url.startsWith("https://")) && url.length() <= 500 && url.indexOf(' ') < 0 &&
         url.indexOf('\n') < 0 && url.indexOf('\r') < 0;
}

void episodesLoadPost(AsyncWebServerRequest *request) {
  String url = request->hasParam("url", true) ? request->getParam("url", true)->value() : "";
  url.trim();
  if (!validFeedUrl(url)) {
    request->send(400, "text/plain", "The feed address has to start with http:// or https:// (at most 500 characters).");
    return;
  }
  if (!episodesCanStart(request)) return;
  if (!episodesLock()) return sendBusy(request);
  episodesUrl = url;
  episodesTitle = "";
  episodesError = "";
  episodesIndex.clear();
  episodesIndex.shrink_to_fit();
  episodesListState = EL_LISTING;
  episodesUnlock();
  if (!episodesStart(new EpisodesOp{true, url})) {
    episodesListState = EL_FAILED;
    request->send(500, "text/plain", "Could not start reading the feed.");
    return;
  }
  request->send(202, "text/plain", "Reading the feed");
}

static const char *episodesListStateName(EpisodesListState s) {
  switch (s) {
    case EL_LISTING: return "listing";
    case EL_LISTED: return "listed";
    case EL_FAILED: return "failed";
    default: return "none";
  }
}

// One page of the list, newest first, with what is already in the album folder.
void episodesListJson(AsyncWebServerRequest *request) {
  int offset = request->hasParam("offset") ? request->getParam("offset")->value().toInt() : 0;
  int limit = request->hasParam("limit") ? request->getParam("limit")->value().toInt() : 20;
  if (offset < 0) offset = 0;
  limit = constrain(limit, 1, EPISODES_PAGE_MAX);
  String folder = currentFolder();

  if (!episodesLock()) return sendBusy(request);
  EpisodesListState state = episodesListState;
  String url = episodesUrl, title = episodesTitle, error = episodesError;
  int total = state == EL_LISTED ? (int)episodesIndex.size() : 0, skipped = episodesSkipped;
  std::vector<uint32_t> offsets;
  for (int i = offset; i < total && i < offset + limit; i++) offsets.push_back(episodesIndex[i].offset);
  episodesUnlock();

  std::vector<EpisodeLine> lines = readEpisodeLines(offsets);
  auto page = std::make_shared<PageBuffer>();
  Print *res = page.get();
  res->printf("{\"state\":\"%s\",\"url\":\"%s\",\"title\":\"%s\",\"error\":\"%s\",\"total\":%d,\"skipped\":%d,\"offset\":%d,\"items\":[",
              episodesListStateName(state), jsonEscape(url).c_str(), jsonEscape(title).c_str(), jsonEscape(error).c_str(),
              total, skipped, offset);
  // s: size as the feed says (often wrong); rs: the real one of an episode already on the card.
  for (size_t i = 0; i < lines.size(); i++) {
    const EpisodeLine &l = lines[i];
    uint32_t real = folder.length() && l.file.length() ? fileSizeOnCard(folder + "/" + l.file) : 0;
    res->printf("%s{\"id\":%d,\"t\":\"%s\",\"d\":\"%s\",\"s\":%u,\"rs\":%u,\"ms\":%u,\"on\":%s}", i ? "," : "",
                offset + (int)i, jsonEscape(l.title).c_str(), jsonEscape(l.when).c_str(), (unsigned)l.size, (unsigned)real,
                (unsigned)(l.seconds * 1000), real ? "true" : "false");
  }
  res->print("]}");
  sendBuffered(request, page, "application/json", true);
}

// Starts loading the episodes with the ids in `ids` ("3,7,12") into the album folder.
void episodesDownloadPost(AsyncWebServerRequest *request) {
  String ids = request->hasParam("ids", true) ? request->getParam("ids", true)->value() : "";
  String url = request->hasParam("url", true) ? request->getParam("url", true)->value() : "";
  String folder = currentFolder();
  if (folder.isEmpty()) {
    request->send(409, "text/plain", "Place an album token on the box first.");
    return;
  }
  if (!episodesCanStart(request)) return;

  std::vector<int> picked;
  std::vector<uint32_t> offsets;
  if (!episodesLock()) return sendBusy(request);
  bool sameList = episodesListState == EL_LISTED && episodesUrl == url;
  for (int start = 0; sameList && start < (int)ids.length() && picked.size() < EPISODES_JOBS_MAX;) {
    int comma = ids.indexOf(',', start);
    if (comma < 0) comma = ids.length();
    int id = comma > start ? ids.substring(start, comma).toInt() : -1;
    if (id >= 0 && id < (int)episodesIndex.size() && std::find(picked.begin(), picked.end(), id) == picked.end()) {
      picked.push_back(id);
      offsets.push_back(episodesIndex[id].offset);
    }
    start = comma + 1;
  }
  episodesUnlock();
  if (!sameList) {
    request->send(409, "text/plain", "The episode list has changed. Show the episodes again.");
    return;
  }
  std::vector<EpisodeLine> lines = readEpisodeLines(offsets);
  std::vector<EpisodeJob> jobs;
  for (size_t i = 0; i < lines.size(); i++) {
    if (lines[i].file.isEmpty() || lines[i].url.isEmpty()) continue;
    EpisodeJob job;
    job.id = picked[i];
    job.title = lines[i].title;
    job.file = lines[i].file;
    job.url = lines[i].url;
    job.size = lines[i].size;
    job.ms = lines[i].seconds * 1000;
    jobs.push_back(job);
  }
  if (jobs.empty()) {
    request->send(400, "text/plain", "No episodes picked.");
    return;
  }

  if (!episodesLock()) return sendBusy(request);
  episodesJobs.swap(jobs);
  episodesFolder = folder;
  episodesCurrent = -1;
  episodesJobState = EJ_RUNNING;
  episodesUnlock();
  if (!episodesStart(new EpisodesOp{false, ""})) {
    episodesJobState = EJ_DONE;
    request->send(500, "text/plain", "Could not start the download.");
    return;
  }
  request->send(202, "text/plain", "Loading the episodes");
}

static const char *episodeJobStatusName(uint8_t s) {
  switch (s) {
    case EJS_LOADING: return "loading";
    case EJS_DONE: return "done";
    case EJS_FAILED: return "failed";
    case EJS_PRESENT: return "present";
    case EJS_CANCELLED: return "cancelled";
    default: return "waiting";
  }
}

// The download job: each episode with its status, and the bytes so far for the progress bar.
void episodesStatusJson(AsyncWebServerRequest *request) {
  if (!episodesLock()) return sendBusy(request);
  EpisodesJobState state = episodesJobState;
  int current = episodesCurrent;
  std::vector<EpisodeJob> jobs = episodesJobs;
  episodesUnlock();

  uint32_t synced = 0, expected = 0;
  bool done = false;
  bool growing = current >= 0 && growingGet(EPISODES_TEMP_FILE, synced, expected, done);
  uint64_t bytes = 0, bytesTotal = 0;
  auto page = std::make_shared<PageBuffer>();
  Print *res = page.get();
  res->printf("{\"state\":\"%s\",\"jobs\":[", state == EJ_RUNNING ? "running" : state == EJ_DONE ? "done"
                                                : state == EJ_CANCELLED                       ? "cancelled"
                                                                                             : "none");
  for (size_t i = 0; i < jobs.size(); i++) {
    const EpisodeJob &j = jobs[i];
    uint32_t size = j.size, loaded = 0;
    if (j.status == EJS_LOADING && growing) {
      loaded = synced;
      if (expected) size = expected;
    } else if (j.status == EJS_DONE || j.status == EJS_PRESENT) {
      loaded = size;
    }
    bytes += loaded;
    bytesTotal += size;
    res->printf("%s{\"id\":%d,\"t\":\"%s\",\"s\":%u,\"ms\":%u,\"l\":%u,\"st\":\"%s\",\"e\":\"%s\"}", i ? "," : "",
                j.id, jsonEscape(j.title).c_str(), (unsigned)size, (unsigned)j.ms, (unsigned)loaded,
                episodeJobStatusName(j.status), jsonEscape(j.error).c_str());
  }
  res->printf("],\"bytes\":%llu,\"bytesTotal\":%llu}", (unsigned long long)bytes, (unsigned long long)bytesTotal);
  sendBuffered(request, page, "application/json", true);
}

void episodesCancelPost(AsyncWebServerRequest *request) {
  if (episodesBusy()) podcastCancel = true;
  request->send(204);
}

void registerEpisodeRoutes(AsyncWebServer &server) {
  server.on("/episodes", HTTP_GET, episodesListJson);
  server.on("/episodes-load", HTTP_POST, episodesLoadPost);
  server.on("/episodes-download", HTTP_POST, episodesDownloadPost);
  server.on("/episodes-status", HTTP_GET, episodesStatusJson);
  server.on("/episodes-cancel", HTTP_POST, episodesCancelPost);
}

// Call once from setupWebServer(). A download cut off by switching the box off leaves its temp file.
void episodesBegin() {
  episodesMutex = xSemaphoreCreateMutex();
  fatRemove(EPISODES_TEMP_FILE);
}

#endif
