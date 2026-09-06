#ifndef MANAGEMENT_WEB_SERVER
#define MANAGEMENT_WEB_SERVER

#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <algorithm>
#include <vector>

#include "web-ui.h"
#include "wifi-setup.h"

#define PAGE_SIZE 10
#define LAST_PLAYED_FILE_NAME "/last_played.txt"

AsyncWebServer server(80);
String webServerFolderPath = "";  // empty until an album token is placed
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

bool renameFolder(const String &oldFolder, String &newFolder) {
  String tag = oldFolder.substring(max(oldFolder.lastIndexOf("_"), (int)oldFolder.length() - 14));
  if (!newFolder.endsWith(tag)) {
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
void setAlbumFolder(const String &path) {
  webServerFolderPath = path;
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

// "/Bibi_Blocksberg_047a8772d42a81" -> "Bibi_Blocksberg" (the token suffix is re-attached by renameFolder)
String folderDisplayName(const String &folder) {
  String name = folder;
  if (name.startsWith("/")) name = name.substring(1);
  int underscore = name.lastIndexOf('_');
  if (underscore > 0 && name.length() - underscore - 1 == 14) name = name.substring(0, underscore);
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
  int trackCount = filesInDir.size();
  uint64_t sdTotal = SD.totalBytes();
  uint64_t sdUsed = SD.usedBytes();
  filesUnlock();
  if (folder.isEmpty()) return request->redirect("/wifi");
  uint64_t sdFree = sdTotal > sdUsed ? sdTotal - sdUsed : 0;

  AsyncResponseStream *res = request->beginResponseStream("text/html");
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
  res->printf("<input class=\"input\" id=\"path\" name=\"path\" type=\"text\" value=\"%s\" required spellcheck=\"false\">", htmlEscape(folderDisplayName(folder)).c_str());
  res->print("<button class=\"btn secondary\" type=\"submit\">Rename</button></div>"
             "<p class=\"hint\">The token suffix stays attached automatically. Spaces and umlauts are converted.</p></form></div>");

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
  res->print("var items=[],total=0,doneBytes=0,startAt=0,samples=[],busy=false,cur=null,cancelled=false;"
             "function esc(s){return String(s).replace(/[&<>\"']/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',\"'\":'&#39;'}[c]})}"
             "function fmt(n){if(n>=1073741824)return (n/1073741824).toFixed(2)+' GB';if(n>=1048576)return (n/1048576).toFixed(1)+' MB';return Math.max(1,Math.round(n/1024))+' kB'}"
             "function fmtT(s){if(s<60)return Math.ceil(s)+' s';var m=Math.floor(s/60);return m+' min '+Math.round(s-m*60)+' s'}"
             "function fp(k){return $('it'+k).querySelector('.fp')}"
             // selection: filter to MP3, build the list, check the selection against the free SD space
             "function setFiles(list){if(busy)return;var a=[],skipped=0;for(var k=0;k<list.length;k++){var f=list[k];if(f.type==='audio/mpeg'||/\\.mp3$/i.test(f.name))a.push(f);else skipped++}"
             "items=a.map(function(f){return{file:f,status:'pending'}});total=0;items.forEach(function(x){total+=x.file.size});doneBytes=0;b.style.width='0';u.textContent='Upload';"
             "ul.hidden=!items.length;ul.innerHTML=items.map(function(x,k){return '<li id=\"it'+k+'\"><i class=\"st\"></i><span class=\"fn\">'+esc(x.file.name)+'</span><span class=\"fp\"></span><span class=\"fz\">'+fmt(x.file.size)+'</span></li>'}).join('');"
             "if(!items.length){ph.textContent=skipped?'Only MP3 files can be uploaded':'No files selected';ph.className='hint';u.disabled=true;return}"
             "var s=items.length+' file'+(items.length>1?'s':'')+' \\u00b7 '+fmt(total)+(skipped?' \\u00b7 '+skipped+' skipped (not MP3)':'');"
             "if(total>SD_FREE){ph.textContent=s+' \\u00b7 not enough space on the SD card ('+fmt(SD_FREE)+' free)';ph.className='hint danger';u.disabled=true}"
             "else{ph.textContent=s+' \\u00b7 '+fmt(SD_FREE-total)+' will stay free';ph.className='hint';u.disabled=false}}"
             "function setSt(k,st,txt){items[k].status=st;var li=$('it'+k);li.firstChild.className='st '+({uploading:'up',saving:'sv',done:'ok',failed:'err'}[st]||'');fp(k).textContent=txt||''}"
             // overall bar plus rate/ETA from a 4 s sliding window, shown after 2.5 s
             "function progress(c){var sent=doneBytes+c,pct=total?Math.min(100,Math.round(sent/total*100)):0;b.style.width=pct+'%';"
             "var now=Date.now();samples.push([now,sent]);while(samples.length>2&&now-samples[0][0]>4000)samples.shift();"
             "if(samples.length>1){var a=samples[0],e=samples[samples.length-1],rate=e[0]>a[0]?(e[1]-a[1])/((e[0]-a[0])/1000):0;"
             "if(now-startAt>2500&&rate>0)r.textContent=fmt(rate)+'/s \\u00b7 ~'+fmtT((total-sent)/rate)+' left'}"
             "return pct}"
             // one file: upload.onprogress = bytes on the wire, upload.onload = all sent (box still saving), onload = box answered
             "function uploadOne(k,cb){var x=items[k],xhr=new XMLHttpRequest();cur=xhr;xhr.open('POST','/upload');setSt(k,'uploading','0 %');"
             "xhr.upload.onprogress=function(e){if(!e.lengthComputable)return;var pct=progress(e.loaded);t.textContent='Uploading '+(k+1)+' / '+items.length+' \\u00b7 '+fmt(doneBytes+e.loaded)+' of '+fmt(total)+' \\u00b7 '+pct+' %';fp(k).textContent=Math.round(e.loaded/e.total*100)+' %'};"
             "xhr.upload.onload=function(){setSt(k,'saving','saving');t.textContent='Saving '+(k+1)+' / '+items.length+' on the SD card\\u2026'};"
             "xhr.onload=function(){cur=null;if(xhr.status>=200&&xhr.status<300){doneBytes+=x.file.size;setSt(k,'done');progress(0);cb(true)}else{setSt(k,'failed','HTTP '+xhr.status);cb(false)}};"
             "xhr.onerror=xhr.ontimeout=function(){cur=null;setSt(k,'failed','failed');cb(false)};"
             "xhr.onabort=function(){cur=null;setSt(k,'failed','cancelled');cb(false)};"
             "var d=new FormData();d.append('files',x.file);xhr.send(d)}"
             // modal: the <dialog> blocks the page behind a blurred backdrop; Esc is ignored while busy
             "function openModal(){mslot.appendChild(ul);ul.hidden=false;if(dlg.showModal){if(!dlg.open)dlg.showModal()}else dlg.setAttribute('open','')}"
             "function closeModal(){if(dlg.close){if(dlg.open)dlg.close()}else dlg.removeAttribute('open');slot.appendChild(ul)}"
             "dlg.addEventListener('cancel',function(e){if(busy)e.preventDefault()});dlg.addEventListener('close',function(){slot.appendChild(ul)});"
             "window.addEventListener('beforeunload',function(e){if(busy){e.preventDefault();e.returnValue=''}});"
             // run everything not yet done, sequentially; failed files stay in the list for a retry
             "function run(){var q=[];items.forEach(function(x,k){if(x.status!=='done')q.push(k)});if(!q.length||busy)return;"
             "busy=true;cancelled=false;u.disabled=true;i.disabled=true;rb.hidden=true;mx.hidden=true;ms.hidden=true;mc.hidden=false;mh.hidden=false;"
             "mt.textContent='Uploading';t.textContent='Starting\\u2026';r.textContent='';startAt=Date.now();samples=[];"
             "doneBytes=0;items.forEach(function(x){if(x.status==='done')doneBytes+=x.file.size});progress(0);openModal();"
             "var failed=0;(function next(){if(cancelled){finish(failed);return}var k=q.shift();if(k===undefined){finish(failed);return}uploadOne(k,function(ok){if(!ok)failed++;next()})})()}"
             "function finish(failed){busy=false;i.disabled=false;r.textContent='';mc.hidden=true;mh.hidden=true;"
             "var pending=items.filter(function(x){return x.status!=='done'}).length;u.disabled=!pending;u.textContent=pending&&pending<items.length?'Upload remaining':'Upload';"
             "if(cancelled){mt.textContent='Upload cancelled';t.textContent=(items.length-pending)+' of '+items.length+' uploaded \\u00b7 '+fmt(doneBytes);rb.textContent='Continue upload';rb.hidden=false;mx.hidden=false}"
             "else if(failed){mt.textContent='Upload incomplete';t.textContent=failed+' of '+items.length+' failed \\u00b7 '+fmt(doneBytes)+' uploaded. Check the connection, then retry.';rb.textContent='Retry failed';rb.hidden=false;mx.hidden=false}"
             "else{mt.textContent='Done';t.textContent=items.length+' file'+(items.length>1?'s':'')+' \\u00b7 '+fmt(total)+' uploaded';ms.hidden=false;setTimeout(function(){location.href='/'},1500)}}"
             "i.onchange=function(){setFiles(i.files)};"
             "['dragenter','dragover'].forEach(function(ev){z.addEventListener(ev,function(e){e.preventDefault();if(!busy)z.classList.add('over')})});"
             "['dragleave','drop'].forEach(function(ev){z.addEventListener(ev,function(e){e.preventDefault();z.classList.remove('over')})});"
             "z.addEventListener('drop',function(e){setFiles(e.dataTransfer.files)});"
             "document.addEventListener('dragover',function(e){e.preventDefault()});document.addEventListener('drop',function(e){e.preventDefault()});"
             "u.onclick=run;rb.onclick=run;mc.onclick=function(){if(!busy)return;cancelled=true;if(cur)cur.abort()};mx.onclick=closeModal;ms.onclick=function(){location.href='/'};})()</script>");

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

  AsyncResponseStream *res = request->beginResponseStream("text/html");
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

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    Serial.println("Upload Start: " + String(filename));
    // open the file on first call and store the file handle in the request object
    if (filesLock()) {
      request->_tempFile = SD.open("/temp", "w");
      filesUnlock();
    }
  }

  if (len && request->_tempFile) {
    // stream the incoming chunk to the opened file
    if (filesLock()) {
      request->_tempFile.write(data, len);
      filesUnlock();
    }
  }

  if (final) {
    Serial.println("Upload Complete: " + String(filename) + ",size: " + String(index + len));
    // close the file handle as the upload is now done
    filename = sanitizeFilename(filename);
    if (filesLock()) {
      request->_tempFile.close();
      if (webServerFolderPath.isEmpty()) {
        SD.remove("/temp");  // token was removed meanwhile
      } else {
        SD.rename("/temp", webServerFolderPath + "/" + filename);
        filesInDir.erase(std::remove(filesInDir.begin(), filesInDir.end(), filename), filesInDir.end());
        filesInDir.push_back(filename);
        deleteIndexFiles(webServerFolderPath);
      }
      filesUnlock();
    }
  }
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
    if (!filesLock()) return sendBusy(request);
    if (!webServerFolderPath.isEmpty()) {
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

  // Joins the saved home network or opens the hotspot; see wifi-setup.h.
  netBegin();

  Serial.println("starting management server");
  server.on("/home", HTTP_GET, home);
  server.on("/", HTTP_GET, listFiles);
  server.on("/css", HTTP_GET, cssHandler);

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) { request->send(200, "text/plain", "File uploaded successfully"); }, handleUpload);
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
