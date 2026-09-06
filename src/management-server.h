#ifndef MANAGEMENT_WEB_SERVER
#define MANAGEMENT_WEB_SERVER

#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <algorithm>
#include <vector>

#include "web-ui.h"
#include "wifi-setup.h"

#define PAGE_SIZE 10
#define LAST_PLAYED_FILE_NAME "/last_played.txt"

AsyncWebServer server(80);
String webServerFolderPath = "";  // empty until an album token is placed
String webServerAlbumTag = "";    // RFID id of that token; folders are named <name>_<tag>
std::vector<String> filesInDir;
float *batteryAdcValueManagementServer;

// The loop task switches albums (token swap) while web handlers on the AsyncTCP
// task read the file list and touch the SD card, so both go through this mutex.
SemaphoreHandle_t filesMutex = nullptr;

bool filesLock(uint32_t waitMs = 3000) {
  return filesMutex && xSemaphoreTake(filesMutex, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}

void filesUnlock() { xSemaphoreGive(filesMutex); }

// Snapshot of the selected album folder ("" = none). Safe from any task.
String currentFolder() {
  String copy;
  if (filesLock()) {
    copy = webServerFolderPath;
    filesUnlock();
  }
  return copy;
}

// ---------------------------------------------------------------------------
// SD helpers (caller holds filesMutex when running on the web server task)
// ---------------------------------------------------------------------------

String getLastPlayedFileName(const String &folder) {
  if (!SD.exists(folder + LAST_PLAYED_FILE_NAME)) {
    return "";
  }

  File file = SD.open(folder + LAST_PLAYED_FILE_NAME);

  String fileName;
  while (file.available()) {
    fileName += (char)file.read();
  }
  file.close();
  return fileName;
}

void deleteLastPlayedFile(const String &folder) {
  if (SD.exists(folder + LAST_PLAYED_FILE_NAME)) {
    SD.remove(folder + LAST_PLAYED_FILE_NAME);
  }
}

void deleteIndexFiles(const String &folder) {
  if (SD.exists(folder + "/index.txt")) {
    SD.remove(folder + "/index.txt");
  }
}

String sanitizeFilename(const String &input) {
  String result = "";
  unsigned int i = 0;
  while (i < input.length()) {
    String ch = input.substring(i, i + 1);

    // Handle multibyte characters
    if ((ch[0] & 0xC0) == 0xC0 && (i + 1 < input.length())) {
      ch = input.substring(i, i + 2);  // UTF-8 2-byte sequence
      i++;
    }

    if (ch == " ") {
      result += "_";
    } else if (ch == "\xc3\x9c") {
      result += "Ue";
    } else if (ch == "\xc3\xbc") {
      result += "ue";
    } else if (ch == "\xc3\x84") {
      result += "Ae";
    } else if (ch == "\xc3\xa4") {
      result += "ae";
    } else if (ch == "\xc3\x96") {
      result += "Oe";
    } else if (ch == "\xc3\xb6") {
      result += "oe";
    } else if (ch == "ß") {
      result += "ss";
    } else if (isalnum(ch[0]) || ch == "_" || ch == "-" || ch == "(" || ch == ")" || ch == "[" || ch == "]" || ch == "." || ch == "," || ch == "+" || ch == "&") {
      result += ch;  // Keep allowed characters
    } else {
      // Replace all other invalid characters with an underscore
      result += '_';
    }
    i++;
  }

  return result;
}

// Moves the album folder to "<newFolder>_<tag>"; the token id is always appended
// here, the UI never shows it. Caller holds filesMutex.
bool renameFolder(const String &oldFolder, String &newFolder) {
  const String &tag = webServerAlbumTag;
  if (!tag.isEmpty() && !newFolder.endsWith(tag)) {
    newFolder += "_" + tag;
  }

  if (newFolder.startsWith("/")) {
    newFolder = newFolder.substring(1);
  }

  newFolder = "/" + sanitizeFilename(newFolder);

  // Make sure the new folder exists (it will be created if not)
  if (newFolder.lastIndexOf('/') > 0 || !SD.mkdir(newFolder)) {
    Serial.print("Failed to create new folder! ");
    Serial.println(newFolder);
    return false;
  }

  // List the files in the old folder
  File oldDir = SD.open(oldFolder);
  if (oldDir) {
    File entry = oldDir.openNextFile();
    while (entry) {
      String oldFile = oldFolder + "/" + entry.name();
      String newFile = newFolder + "/" + entry.name();

      // Rename the file by changing its path
      if (SD.rename(oldFile, newFile)) {
        Serial.print("Renamed: ");
        Serial.print(oldFile);
        Serial.print(" -> ");
        Serial.println(newFile);
      } else {
        Serial.print("Failed to rename: ");
        Serial.println(oldFile);
      }

      entry = oldDir.openNextFile();
    }
    oldDir.close();

    // Optionally remove the old folder if it's empty
    if (SD.rmdir(oldFolder)) {
      Serial.println("Old folder removed.");
    } else {
      Serial.println("Failed to remove old folder.");
    }

    return true;
  } else {
    Serial.println("Failed to open old folder.");
    return false;
  }
}

// Scans webServerFolderPath for .mp3 files into filesInDir. Caller holds filesMutex.
void prepareFilesInDir() {
  filesInDir.clear();
  if (webServerFolderPath.isEmpty()) return;

  File dir = SD.open(webServerFolderPath);

  while (File file = dir.openNextFile()) {
    esp_task_wdt_reset();
    if (!file.isDirectory() && String(file.name()).endsWith(".mp3")) {
      filesInDir.push_back(String(file.name()));  // Store the filesInDir
    }
    file.close();
  }
  dir.close();
  Serial.println(String(filesInDir.size()) + " files loaded from " + webServerFolderPath);
}

// ---------------------------------------------------------------------------
// Album selection (loop task, driven by the RFID token)
// ---------------------------------------------------------------------------

// Caller holds filesMutex (see PlayerI2S::selectAlbumForManagement, which also
// resolves the folder on the SD card under the same lock).
void setAlbumFolder(const String &path, const String &tag) {
  webServerFolderPath = path;
  webServerAlbumTag = tag;
  prepareFilesInDir();
}

// ---------------------------------------------------------------------------
// Presentation helpers
// ---------------------------------------------------------------------------

// Subtitle shown under the box name on every page.
String albumSubtitle() {
  String folder = currentFolder();
  if (folder.isEmpty()) return "Place an album token on the box to manage its files";
  return "Album folder <code>" + htmlEscape(folder) + "</code>";
}

// Files and Upload only make sense with an album token in place.
bool navVisible(const NavItem &item) {
  if (strcmp(item.key, "wifi") == 0) return true;
  return !currentFolder().isEmpty();
}

// Shown on every page while only the management token is on the box. Polls /album
// and jumps to the file list as soon as an album token is recognised.
void albumNotice(AsyncResponseStream *res) {
  if (!currentFolder().isEmpty()) return;
  res->print("<div class=\"notice\"><i class=\"ic ic-warn\"></i><span><b>Which album?</b> Only the management token is on the box. "
             "Swap it for the token of the album you want to edit. Files and Upload appear here automatically.</span></div>"
             "<script>(function(){function c(){fetch('/album',{cache:'no-store'}).then(function(r){return r.json()}).then(function(d){if(d.album)location.href='/';else setTimeout(c,2000)}).catch(function(){setTimeout(c,3000)})}setTimeout(c,2000)})()</script>");
}

void sendBusy(AsyncWebServerRequest *request);

void albumJson(AsyncWebServerRequest *request) {
  if (!filesLock()) return sendBusy(request);
  String json = "{\"album\":\"" + jsonEscape(webServerFolderPath) + "\",\"tracks\":" + String(filesInDir.size()) + "}";
  filesUnlock();
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

// "/Bibi_Blocksberg_047a8772d42a81" -> "Bibi_Blocksberg", "/047a8772d42a81" -> "" (unnamed album).
// The token id never shows up in the UI; renameFolder re-attaches it.
String folderDisplayName(const String &folder, const String &tag) {
  String name = folder;
  if (name.startsWith("/")) name = name.substring(1);
  if (tag.isEmpty()) return name;
  if (name == tag) return "";
  if (name.endsWith("_" + tag)) return name.substring(0, name.length() - tag.length() - 1);
  return name;
}

// Same conversion as getBattery() in main.ino, kept here because only the raw ADC value is shared.
int batteryMilliVolts() {
  return ::map((uint)*batteryAdcValueManagementServer, 1994, 2390, 3685, 4322);
}

int batteryPercent() {
  return constrain(::map(batteryMilliVolts(), 3200, 4200, 0, 100), 0, 100);
}

// 3.2 GB / 512 MB
String formatBytes(uint64_t bytes) {
  const double GB = 1024.0 * 1024.0 * 1024.0;
  const double MB = 1024.0 * 1024.0;
  char buf[24];
  if (bytes >= GB) {
    snprintf(buf, sizeof(buf), "%.1f GB", bytes / GB);
  } else {
    snprintf(buf, sizeof(buf), "%.0f MB", bytes / MB);
  }
  return String(buf);
}

void sendBusy(AsyncWebServerRequest *request) {
  request->send(503, "text/plain", "The box is busy, please retry.");
}

// ---------------------------------------------------------------------------
// Pages
// ---------------------------------------------------------------------------

void home(AsyncWebServerRequest *request) {
  if (!filesLock()) return sendBusy(request);
  String folder = webServerFolderPath;
  String tag = webServerAlbumTag;
  int trackCount = filesInDir.size();
  uint64_t sdTotal = SD.totalBytes();
  uint64_t sdUsed = SD.usedBytes();
  filesUnlock();
  if (folder.isEmpty()) return request->redirect("/wifi");
  uint64_t sdFree = sdTotal > sdUsed ? sdTotal - sdUsed : 0;

  AsyncResponseStream *res = request->beginResponseStream("text/html", 12288);
  pageHead(res, request, "upload");

  // Status card
  int percent = batteryPercent();
  const char *pillClass = percent > 50 ? "ok" : percent > 20 ? "warn" : "danger";
  res->print("<div class=\"card\"><div class=\"card-head\"><h2>Status</h2>");
  res->printf("<span class=\"pill %s\"><span class=\"dot\"></span>Battery %d&thinsp;%%</span></div>", pillClass, percent);
  res->print("<dl class=\"kv\">");
  res->printf("<dt>Battery</dt><dd>%d mV &middot; ADC %d</dd>", batteryMilliVolts(), (int)*batteryAdcValueManagementServer);
  res->printf("<dt>Album folder</dt><dd>%s</dd>", htmlEscape(folder).c_str());
  res->printf("<dt>Tracks</dt><dd>%d</dd>", trackCount);
  res->printf("<dt>SD card</dt><dd>%s free of %s</dd>", formatBytes(sdFree).c_str(), formatBytes(sdTotal).c_str());
  res->print("</dl></div>");

  // Upload card: dropzone + selection summary. The upload itself runs in a modal
  // <dialog> with a backdrop so it is obvious that leaving the page cancels it.
  res->print("<div class=\"card\"><div class=\"card-head\"><h2>Upload</h2><span class=\"muted small\">MP3 only</span></div>"
             "<label class=\"dropzone\" id=\"dz\"><input type=\"file\" id=\"f\" accept=\"audio/mpeg,.mp3\" multiple>"
             "<strong>Choose MP3 files</strong><span>or drag &amp; drop them here &middot; uploaded one after another to the album folder</span></label>"
             "<div id=\"slot\"><ul class=\"ulist\" id=\"ul\" hidden></ul></div>"
             "<div class=\"actions between\"><span class=\"hint\" id=\"ph\">No files selected</span>"
             "<button class=\"btn\" type=\"button\" id=\"ub\" disabled>Upload</button></div></div>");

  // Rename card
  res->print("<div class=\"card\"><div class=\"card-head\"><h2>Rename Folder</h2></div>"
             "<form method=\"POST\" action=\"/setFolderPath\" accept-charset=\"UTF-8\" class=\"field\">"
             "<label for=\"path\">Folder name</label><div class=\"row-inline\">");
  res->printf("<input class=\"input\" id=\"path\" name=\"path\" type=\"text\" value=\"%s\" placeholder=\"Album name\" required spellcheck=\"false\">", htmlEscape(folderDisplayName(folder, tag)).c_str());
  res->print("<button class=\"btn secondary\" type=\"submit\">Rename</button></div>"
             "<p class=\"hint\">The token id is kept out of sight and attached automatically. Spaces and umlauts are converted.</p></form></div>");

  // Upload modal (the file list <ul> moves in here while uploading and back afterwards)
  res->print("<dialog class=\"modal\" id=\"dlg\"><div class=\"card\">"
             "<div class=\"card-head\"><h2 id=\"mt\">Uploading</h2><span class=\"hint mono\" id=\"pr\"></span></div>"
             "<div class=\"progress\"><i id=\"bar\"></i></div>"
             "<p class=\"hint mono\" id=\"pt\"></p>"
             "<div id=\"mslot\"></div>"
             "<div class=\"notice\" id=\"mh\"><i class=\"ic ic-warn\"></i><span>Keep this page open. Navigating away or closing the browser cancels the upload.</span></div>"
             "<div class=\"actions end\">"
             "<button class=\"btn destructive\" type=\"button\" id=\"mc\">Cancel upload</button>"
             "<button class=\"btn secondary\" type=\"button\" id=\"mx\" hidden>Close</button>"
             "<button class=\"btn\" type=\"button\" id=\"rb\" hidden>Retry failed</button>"
             "<button class=\"btn\" type=\"button\" id=\"ms\" hidden>Show files</button>"
             "</div></div></dialog>");

  // Upload script: one file per request, /temp on the SD card is shared, so uploads must not overlap.
  // XMLHttpRequest instead of fetch because only it reports upload progress per byte.
  char sdFreeBuf[24];
  snprintf(sdFreeBuf, sizeof(sdFreeBuf), "%llu", (unsigned long long)sdFree);
  res->print("<script>(function(){var $=function(id){return document.getElementById(id)},i=$('f'),z=$('dz'),b=$('bar'),t=$('pt'),r=$('pr'),ul=$('ul'),ph=$('ph'),u=$('ub'),rb=$('rb'),"
             "dlg=$('dlg'),mt=$('mt'),mh=$('mh'),mc=$('mc'),mx=$('mx'),ms=$('ms'),slot=$('slot'),mslot=$('mslot');");
  res->printf("var SD_FREE=%s;", sdFreeBuf);
  res->print("var PAR=2,items=[],total=0,doneBytes=0,startAt=0,samples=[],busy=false,cancelled=false,q=[],active=0,failed=0,inflight={};"
             "function esc(s){return String(s).replace(/[&<>\"']/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',\"'\":'&#39;'}[c]})}"
             "function fmt(n){if(n>=1073741824)return (n/1073741824).toFixed(2)+' GB';if(n>=1048576)return (n/1048576).toFixed(1)+' MB';return Math.max(1,Math.round(n/1024))+' kB'}"
             "function fmtT(s){if(s<60)return Math.ceil(s)+' s';var m=Math.floor(s/60);return m+' min '+Math.round(s-m*60)+' s'}"
             "function fp(k){return $('it'+k).querySelector('.fp')}"
             // selection: filter to MP3, build the list, check the selection against the free SD space
             "function setFiles(list){if(busy)return;var a=[],skipped=0;for(var k=0;k<list.length;k++){var f=list[k];if(f.type==='audio/mpeg'||/\\.mp3$/i.test(f.name))a.push(f);else skipped++}"
             "items=a.map(function(f){return{file:f,status:'pending',loaded:0}});total=0;items.forEach(function(x){total+=x.file.size});doneBytes=0;b.style.width='0';u.textContent='Upload';"
             "ul.hidden=!items.length;ul.innerHTML=items.map(function(x,k){return '<li id=\"it'+k+'\"><i class=\"st\"></i><span class=\"fn\">'+esc(x.file.name)+'</span><span class=\"fp\"></span><span class=\"fz\">'+fmt(x.file.size)+'</span></li>'}).join('');"
             "if(!items.length){ph.textContent=skipped?'Only MP3 files can be uploaded':'No files selected';ph.className='hint';u.disabled=true;return}"
             "var s=items.length+' file'+(items.length>1?'s':'')+' \\u00b7 '+fmt(total)+(skipped?' \\u00b7 '+skipped+' skipped (not MP3)':'');"
             "if(total>SD_FREE){ph.textContent=s+' \\u00b7 not enough space on the SD card ('+fmt(SD_FREE)+' free)';ph.className='hint danger';u.disabled=true}"
             "else{ph.textContent=s+' \\u00b7 '+fmt(SD_FREE-total)+' will stay free';ph.className='hint';u.disabled=false}}"
             "function setSt(k,st,txt){items[k].status=st;var li=$('it'+k);li.firstChild.className='st '+({uploading:'up',saving:'sv',done:'ok',failed:'err'}[st]||'');fp(k).textContent=txt||''}"
             // overall bar, text and rate/ETA (4 s sliding window, shown after 2.5 s) across all files in flight
             "function progressAll(){var sent=doneBytes,done=0,act=0;items.forEach(function(x){if(x.status==='done')done++;else if(x.status==='uploading'||x.status==='saving'){act++;sent+=x.loaded}});"
             "var pct=total?Math.min(100,Math.round(sent/total*100)):0;b.style.width=pct+'%';"
             "t.textContent=(act?'Uploading \\u00b7 ':'')+done+' of '+items.length+' done'+(act>1?' \\u00b7 '+act+' at once':'')+' \\u00b7 '+fmt(sent)+' of '+fmt(total)+' \\u00b7 '+pct+' %';"
             "var now=Date.now();samples.push([now,sent]);while(samples.length>2&&now-samples[0][0]>4000)samples.shift();"
             "if(samples.length>1){var a=samples[0],e=samples[samples.length-1],rate=e[0]>a[0]?(e[1]-a[1])/((e[0]-a[0])/1000):0;"
             "if(now-startAt>2500&&rate>0)r.textContent=fmt(rate)+'/s \\u00b7 ~'+fmtT((total-sent)/rate)+' left'}}"
             // one file: upload.onprogress = bytes on the wire, upload.onload = all sent (box still saving), onload = box answered
             "function uploadOne(k,cb){var x=items[k],xhr=new XMLHttpRequest();inflight[k]=xhr;x.loaded=0;xhr.open('POST','/upload?name='+encodeURIComponent(x.file.name));xhr.setRequestHeader('Content-Type','application/octet-stream');setSt(k,'uploading','0 %');"
             "xhr.upload.onprogress=function(e){if(!e.lengthComputable)return;x.loaded=e.loaded;progressAll();fp(k).textContent=Math.round(e.loaded/e.total*100)+' %'};"
             "xhr.upload.onload=function(){x.loaded=x.file.size;setSt(k,'saving','saving')};"
             "function end(ok,st,txt){delete inflight[k];if(ok)doneBytes+=x.file.size;x.loaded=0;setSt(k,st,txt);progressAll();cb(ok)}"
             "xhr.onload=function(){if(xhr.status>=200&&xhr.status<300)end(true,'done');else end(false,'failed','HTTP '+xhr.status)};"
             "xhr.onerror=xhr.ontimeout=function(){end(false,'failed','failed')};"
             "xhr.onabort=function(){end(false,'failed','cancelled')};"
             "xhr.send(x.file)}"
             // modal: the <dialog> blocks the page behind a blurred backdrop; Esc is ignored while busy
             "function openModal(){mslot.appendChild(ul);ul.hidden=false;if(dlg.showModal){if(!dlg.open)dlg.showModal()}else dlg.setAttribute('open','')}"
             "function closeModal(){if(dlg.close){if(dlg.open)dlg.close()}else dlg.removeAttribute('open');slot.appendChild(ul)}"
             "dlg.addEventListener('cancel',function(e){if(busy)e.preventDefault()});dlg.addEventListener('close',function(){slot.appendChild(ul)});"
             "window.addEventListener('beforeunload',function(e){if(busy){e.preventDefault();e.returnValue=''}});"
             // run everything not yet done, PAR files at a time: each connection is capped by the box's
             // small TCP receive window (5.7 kB), so parallel connections multiply the throughput
             "function run(){q=[];items.forEach(function(x,k){if(x.status!=='done')q.push(k)});if(!q.length||busy)return;"
             "busy=true;cancelled=false;failed=0;active=0;u.disabled=true;i.disabled=true;rb.hidden=true;mx.hidden=true;ms.hidden=true;mc.hidden=false;mh.hidden=false;"
             "mt.textContent='Uploading';t.textContent='Starting\\u2026';r.textContent='';startAt=Date.now();samples=[];"
             "doneBytes=0;items.forEach(function(x){if(x.status==='done')doneBytes+=x.file.size});progressAll();openModal();pump()}"
             "function pump(){while(!cancelled&&active<PAR&&q.length){active++;uploadOne(q.shift(),function(ok){active--;if(!ok)failed++;if(active===0&&(cancelled||!q.length))finish(failed);else pump()})}}"
             "function finish(failed){busy=false;i.disabled=false;r.textContent='';mc.hidden=true;mh.hidden=true;"
             "var pending=items.filter(function(x){return x.status!=='done'}).length;u.disabled=!pending;u.textContent=pending&&pending<items.length?'Upload remaining':'Upload';"
             "if(cancelled){mt.textContent='Upload cancelled';t.textContent=(items.length-pending)+' of '+items.length+' uploaded \\u00b7 '+fmt(doneBytes);rb.textContent='Continue upload';rb.hidden=false;mx.hidden=false}"
             "else if(failed){mt.textContent='Upload incomplete';t.textContent=failed+' of '+items.length+' failed \\u00b7 '+fmt(doneBytes)+' uploaded. Check the connection, then retry.';rb.textContent='Retry failed';rb.hidden=false;mx.hidden=false}"
             "else{mt.textContent='Done';t.textContent=items.length+' file'+(items.length>1?'s':'')+' \\u00b7 '+fmt(total)+' uploaded';ms.hidden=false;mx.hidden=false}}"
             "i.onchange=function(){setFiles(i.files)};"
             "['dragenter','dragover'].forEach(function(ev){z.addEventListener(ev,function(e){e.preventDefault();if(!busy)z.classList.add('over')})});"
             "['dragleave','drop'].forEach(function(ev){z.addEventListener(ev,function(e){e.preventDefault();z.classList.remove('over')})});"
             "z.addEventListener('drop',function(e){setFiles(e.dataTransfer.files)});"
             "document.addEventListener('dragover',function(e){e.preventDefault()});document.addEventListener('drop',function(e){e.preventDefault()});"
             "u.onclick=run;rb.onclick=run;mc.onclick=function(){if(!busy)return;cancelled=true;Object.keys(inflight).forEach(function(k){inflight[k].abort()})};mx.onclick=closeModal;ms.onclick=function(){location.href='/'};})()</script>");

  pageFoot(res);
  request->send(res);
}

void listFiles(AsyncWebServerRequest *request) {
  int page = 0;
  if (request->hasParam("page")) {
    page = request->getParam("page")->value().toInt();
  }
  if (page < 0) page = 0;

  // Snapshot the page of files under the lock, render afterwards.
  if (!filesLock()) return sendBusy(request);
  String folder = webServerFolderPath;
  std::sort(filesInDir.begin(), filesInDir.end());
  int total = filesInDir.size();
  int first = std::min(page * PAGE_SIZE, total);
  int last = std::min(first + PAGE_SIZE, total);
  std::vector<String> pageFiles(filesInDir.begin() + first, filesInDir.begin() + last);
  String lastPlayedFileName = folder.isEmpty() ? "" : getLastPlayedFileName(folder);
  filesUnlock();
  if (folder.isEmpty()) return request->redirect("/wifi");

  AsyncResponseStream *res = request->beginResponseStream("text/html", 8192);
  pageHead(res, request, "files");

  res->print("<div class=\"card flush\"><div class=\"card-head\"><h2>Files</h2>");
  res->printf("<span class=\"pill neutral mono\">%d track%s</span></div>", total, total == 1 ? "" : "s");
  res->print("<p class=\"muted small lead\">Tap &#9654; to make a track the one that plays next when the token is placed.</p>");

  if (total == 0) {
    res->print("<p class=\"empty\">No tracks yet. Upload some MP3 files.</p>");
  } else {
    res->print("<ul class=\"list\">");
    for (const String &fileName : pageFiles) {
      String escaped = htmlEscape(fileName);
      bool isNext = lastPlayedFileName.endsWith(fileName);

      String display = fileName;
      String ext = "";
      int dot = display.lastIndexOf('.');
      if (dot > 0) {
        ext = display.substring(dot);
        display = display.substring(0, dot);
      }
      display.replace('_', ' ');

      res->printf("<li><form method=\"POST\" action=\"/select\"><input type=\"hidden\" name=\"filename\" value=\"%s\">"
                  "<button class=\"icon-btn%s\" type=\"submit\" aria-label=\"Play next\"><i class=\"ic ic-play\"></i></button></form>",
                  escaped.c_str(), isNext ? " playing" : "");
      res->printf("<span class=\"name\">%s<span class=\"ext\">%s</span>%s</span>",
                  htmlEscape(display).c_str(), htmlEscape(ext).c_str(), isNext ? "<span class=\"meta\">Plays next</span>" : "");
      res->printf("<form method=\"POST\" action=\"/delete\" onsubmit=\"return confirm('Delete %s?')\"><input type=\"hidden\" name=\"filename\" value=\"%s\">"
                  "<button class=\"icon-btn destructive\" type=\"submit\" aria-label=\"Delete\"><i class=\"ic ic-trash\"></i></button></form></li>",
                  escaped.c_str(), escaped.c_str());
    }
    res->print("</ul>");
  }

  if (total > PAGE_SIZE) {
    res->print("<div class=\"pager\">");
    if (page > 0) {
      res->printf("<a class=\"btn secondary small\" href=\"/?page=%d\">Previous</a>", page - 1);
    } else {
      res->print("<span class=\"btn secondary small\" aria-disabled=\"true\">Previous</span>");
    }
    res->printf("<span class=\"mono\">%d&ndash;%d of %d</span>", total ? first + 1 : 0, last, total);
    if (last < total) {
      res->printf("<a class=\"btn secondary small\" href=\"/?page=%d\">Next</a>", page + 1);
    } else {
      res->print("<span class=\"btn secondary small\" aria-disabled=\"true\">Next</span>");
    }
    res->print("</div>");
  }
  res->print("</div>");

  pageFoot(res);
  request->send(res);
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Upload
//
// The file arrives as raw request body (no byte-wise multipart parsing). The
// network task only copies it into 8 kB buffers from a small pool and hands
// full buffers to a dedicated SD writer task; it never touches the SD card
// itself. That keeps AsyncTCP responsive while several uploads are in flight
// (blocked in SD writes it let its receive timeout reset the other
// connections) and lets SD writes overlap with receiving.
// ---------------------------------------------------------------------------

#define UPLOAD_BUF_SIZE 8192   // 4 x 8 kB pool: the management-mode heap has ~115 kB free, pages need ~25 kB to render
#define UPLOAD_BUF_COUNT 4          // pool shared by all uploads in flight
#define UPLOAD_CLOSE_TIMEOUT_MS 15000
#define UPLOAD_RX_TIMEOUT_S 60      // ESPAsyncWebServer's default of 3 s is too short for a stalled TCP stream

// One upload. Owned by the writer task from OPEN until it hands it back with
// the CLOSE result (then the request deletes it) or cleans it up after ABORT.
struct UploadCtx {
  File file;
  String tmpPath;
  String filename;
  size_t written = 0;
  unsigned long startedAt = 0;
  unsigned long sdMs = 0;  // time the writer spent on the SD card for this file
  volatile bool failed = false;
  int result = 500;
  const char *message = "Upload failed";
  SemaphoreHandle_t done = nullptr;
};

enum UploadOp { UP_OPEN,
                UP_DATA,
                UP_CLOSE,
                UP_ABORT };

struct UploadJob {
  UploadOp op;
  UploadCtx *ctx;
  uint8_t *buf;
  size_t len;
};

// Per request, kept in request->_tempObject (a plain malloc block the server free()s).
struct UploadReq {
  UploadCtx *ctx;
  uint8_t *buf;  // buffer currently being filled, nullptr if none
  size_t fill;
  bool finished;  // CLOSE or ABORT already posted
};

QueueHandle_t uploadJobs = nullptr;
QueueHandle_t uploadFreeBufs = nullptr;
static uint32_t uploadSeq = 0;  // AsyncTCP task only

static void uploadPost(UploadOp op, UploadCtx *ctx, uint8_t *buf, size_t len) {
  UploadJob job = {op, ctx, buf, len};
  xQueueSend(uploadJobs, &job, portMAX_DELAY);
}

static void uploadReleaseBuf(uint8_t *buf) {
  if (buf) xQueueSend(uploadFreeBufs, &buf, 0);
}

// Runs on its own task: the only place that writes upload data to the SD card.
static void uploadWriterTask(void *) {
  UploadJob job;
  for (;;) {
    if (xQueueReceive(uploadJobs, &job, portMAX_DELAY) != pdTRUE) continue;
    UploadCtx *c = job.ctx;
    unsigned long t0 = millis();
    switch (job.op) {
      case UP_OPEN:
        if (filesLock(10000)) {
          c->file = SD.open(c->tmpPath, FILE_WRITE);
          filesUnlock();
        }
        if (!c->file) c->failed = true;
        break;

      case UP_DATA:
        if (!c->failed) {
          bool ok = false;
          if (filesLock(10000)) {
            ok = c->file.write(job.buf, job.len) == job.len;
            filesUnlock();
          }
          if (ok) c->written += job.len;
          else c->failed = true;
        }
        uploadReleaseBuf(job.buf);
        break;

      case UP_CLOSE:
        if (filesLock(10000)) {
          c->file.close();
          String target = webServerFolderPath + "/" + c->filename;
          if (c->failed) {
            SD.remove(c->tmpPath);
            c->result = 500;
            c->message = "Writing to the SD card failed";
          } else if (webServerFolderPath.isEmpty()) {
            SD.remove(c->tmpPath);  // token was removed meanwhile
            c->result = 409;
            c->message = "No album selected";
          } else if ((SD.exists(target) && !SD.remove(target)) || !SD.rename(c->tmpPath, target)) {  // overwrite an existing track
            SD.remove(c->tmpPath);
            c->result = 500;
            c->message = "Could not move the file into the album folder";
          } else {
            filesInDir.erase(std::remove(filesInDir.begin(), filesInDir.end(), c->filename), filesInDir.end());
            filesInDir.push_back(c->filename);
            deleteIndexFiles(webServerFolderPath);
            c->result = 200;
            c->message = "File uploaded successfully";
          }
          filesUnlock();
        } else {
          c->result = 503;
          c->message = "The box is busy, please retry.";
        }
        c->sdMs += millis() - t0;
        xSemaphoreGive(c->done);  // the request owns c from here on
        continue;

      case UP_ABORT:  // client vanished: nobody waits for this one
        if (filesLock(10000)) {
          c->file.close();
          SD.remove(c->tmpPath);
          filesUnlock();
        }
        Serial.printf("Upload aborted: %s after %u bytes\n", c->filename.c_str(), (unsigned)c->written);
        vSemaphoreDelete(c->done);
        delete c;
        continue;
    }
    c->sdMs += millis() - t0;
  }
}

// Call once from setupWebServer(): buffer pool + writer task.
void uploadBegin() {
  uploadJobs = xQueueCreate(UPLOAD_BUF_COUNT + 16, sizeof(UploadJob));
  uploadFreeBufs = xQueueCreate(UPLOAD_BUF_COUNT, sizeof(uint8_t *));
  int buffers = 0;
  for (; buffers < UPLOAD_BUF_COUNT; buffers++) {
    uint8_t *buf = (uint8_t *)malloc(UPLOAD_BUF_SIZE);
    if (!buf) break;
    xQueueSend(uploadFreeBufs, &buf, 0);
  }
  xTaskCreatePinnedToCore(uploadWriterTask, "sdwriter", 6144, nullptr, 3, nullptr, 1);
  Serial.printf("upload: %d buffers of %u bytes, free heap %u\n", buffers, (unsigned)UPLOAD_BUF_SIZE, ESP.getFreeHeap());
}

// Body handler (AsyncTCP task): copy into the current buffer, hand full buffers to the writer.
void handleUploadBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  if (index == 0) {
    request->client()->setRxTimeout(UPLOAD_RX_TIMEOUT_S);
    UploadReq *r = (UploadReq *)calloc(1, sizeof(UploadReq));
    if (!r || !uploadJobs) {
      free(r);
      return;  // reported by handleUploadDone
    }
    UploadCtx *c = new UploadCtx();
    c->tmpPath = "/temp_" + String(++uploadSeq);
    c->filename = request->hasParam("name") ? sanitizeFilename(request->getParam("name")->value()) : "";
    c->startedAt = millis();
    c->done = xSemaphoreCreateBinary();
    r->ctx = c;
    request->_tempObject = r;
    request->onDisconnect([r]() {  // fires on every disconnect, also after a normal response
      if (r->finished) return;
      r->finished = true;
      uploadReleaseBuf(r->buf);
      r->buf = nullptr;
      uploadPost(UP_ABORT, r->ctx, nullptr, 0);
    });
    uploadPost(UP_OPEN, c, nullptr, 0);
    Serial.printf("Upload start: %s (%u bytes), free heap %u, largest block %u\n",
                  c->filename.c_str(), (unsigned)total, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  }

  UploadReq *r = (UploadReq *)request->_tempObject;
  if (!r || r->finished || r->ctx->failed) return;

  while (len) {
    if (!r->buf) {
      if (xQueueReceive(uploadFreeBufs, &r->buf, pdMS_TO_TICKS(5000)) != pdTRUE) {  // writer cannot keep up
        r->buf = nullptr;
        r->ctx->failed = true;
        return;
      }
      r->fill = 0;
    }
    size_t n = std::min(len, (size_t)UPLOAD_BUF_SIZE - r->fill);
    memcpy(r->buf + r->fill, data, n);
    r->fill += n;
    data += n;
    len -= n;
    if (r->fill == UPLOAD_BUF_SIZE) {
      uploadPost(UP_DATA, r->ctx, r->buf, r->fill);
      r->buf = nullptr;
      r->fill = 0;
    }
  }
}

// Request handler: runs once the whole body arrived. Flush the rest, let the writer close and move the file.
void handleUploadDone(AsyncWebServerRequest *request) {
  UploadReq *r = (UploadReq *)request->_tempObject;
  if (!r) {
    request->send(507, "text/plain", "Empty upload or not enough memory");
    return;
  }
  r->finished = true;
  UploadCtx *c = r->ctx;

  if (c->filename.isEmpty()) {
    uploadReleaseBuf(r->buf);
    r->buf = nullptr;
    uploadPost(UP_ABORT, c, nullptr, 0);
    request->send(400, "text/plain", "Missing file name");
    return;
  }

  if (r->buf) {
    if (r->fill && !c->failed) uploadPost(UP_DATA, c, r->buf, r->fill);
    else uploadReleaseBuf(r->buf);
    r->buf = nullptr;
  }
  uploadPost(UP_CLOSE, c, nullptr, 0);

  if (xSemaphoreTake(c->done, pdMS_TO_TICKS(UPLOAD_CLOSE_TIMEOUT_MS)) != pdTRUE) {
    // Writer is stuck; the context stays allocated rather than risking a use after free.
    Serial.printf("Upload %s: writer did not finish in time\n", c->filename.c_str());
    request->send(503, "text/plain", "SD card is busy, please retry");
    return;
  }

  unsigned long ms = millis() - c->startedAt;
  Serial.printf("Upload %s: %s, %u bytes in %lu ms (%.2f MB/s), SD busy %lu ms, free heap %u\n",
                c->result == 200 ? "complete" : "failed", c->filename.c_str(), (unsigned)c->written, ms,
                ms ? c->written / 1048.576 / ms : 0.0, c->sdMs, ESP.getFreeHeap());
  request->send(c->result, "text/plain", c->message);
  vSemaphoreDelete(c->done);
  delete c;
}

void handleFileDelete(AsyncWebServerRequest *request) {
  if (request->hasParam("filename", true)) {
    String fileName = request->getParam("filename", true)->value();
    if (!filesLock()) return sendBusy(request);
    if (!webServerFolderPath.isEmpty()) {
      String filePath = webServerFolderPath + "/" + fileName;
      if (SD.exists(filePath)) {
        SD.remove(filePath);
        filesInDir.erase(std::remove(filesInDir.begin(), filesInDir.end(), fileName), filesInDir.end());
        if (getLastPlayedFileName(webServerFolderPath) == filePath) {
          deleteLastPlayedFile(webServerFolderPath);
        }
      }
      deleteIndexFiles(webServerFolderPath);
    }
    filesUnlock();
  }
  request->redirect(request->header("referer"));
}

void handleFileSelect(AsyncWebServerRequest *request) {
  if (request->hasParam("filename", true)) {
    String fileName = request->getParam("filename", true)->value();
    if (!filesLock()) return sendBusy(request);
    if (!webServerFolderPath.isEmpty()) {
      String filePath = webServerFolderPath + "/" + fileName;
      if (SD.exists(filePath)) {
        File file = SD.open(webServerFolderPath + LAST_PLAYED_FILE_NAME, FILE_WRITE);
        if (file) {
          file.print(filePath);
          file.close();
          Serial.println(filePath);
        } else {
          Serial.println("Failed to open file for writing");
        }
      }
    }
    filesUnlock();
  }
  request->redirect(request->header("referer"));
}

void handleFolderPathChange(AsyncWebServerRequest *request) {
  if (request->hasParam("path", true)) {
    String newFolderPath = request->getParam("path", true)->value();
    newFolderPath.trim();
    if (!filesLock()) return sendBusy(request);
    if (!webServerFolderPath.isEmpty() && !newFolderPath.isEmpty()) {
      if (renameFolder(webServerFolderPath, newFolderPath)) webServerFolderPath = newFolderPath;
      deleteLastPlayedFile(webServerFolderPath);
      deleteIndexFiles(webServerFolderPath);
    }
    filesUnlock();
  }
  request->redirect("/");
}

// ---------------------------------------------------------------------------
// Server lifecycle
// ---------------------------------------------------------------------------

// Starts network + web server without an album. Files/Upload become available via selectAlbum().
void setupWebServer() {
  filesMutex = xSemaphoreCreateMutex();
  pageSubtitle = albumSubtitle;
  navItemVisible = navVisible;
  pageNotice = albumNotice;

  // Joins the saved home network or opens the hotspot; see wifi-setup.h.
  netBegin();
  uploadBegin();

  Serial.println("starting management server");
  server.on("/home", HTTP_GET, home);
  server.on("/", HTTP_GET, listFiles);
  server.on("/css", HTTP_GET, cssHandler);
  server.on("/album", HTTP_GET, albumJson);

  server.on("/upload", HTTP_POST, handleUploadDone, nullptr, handleUploadBody);  // raw body, see handleUploadBody
  server.on("/select", HTTP_POST, handleFileSelect);
  server.on("/delete", HTTP_POST, handleFileDelete);
  server.on("/setFolderPath", HTTP_POST, handleFolderPathChange);
  registerWifiRoutes(server);
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("/home");
  });

  server.begin();
}

// Call every loop() iteration while the management server runs; returns the current network mode.
NetMode managementServerLoop() {
  return netLoop();
}

#endif
