#ifndef MANAGEMENT_WEB_SERVER
#define MANAGEMENT_WEB_SERVER

#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include <algorithm>
#include <vector>

#include "web-ui.h"

#define PAGE_SIZE 10
#define LAST_PLAYED_FILE_NAME "/last_played.txt"

DNSServer dnsServer;

AsyncWebServer server(80);
String webServerFolderPath = "/";
std::vector<String> filesInDir;
float *batteryAdcValueManagementServer;

String getLastPlayedFileName() {
  if (!SD.exists(webServerFolderPath + LAST_PLAYED_FILE_NAME)) {
    Serial.println("Failed to open file for reading");
    return "";
  }

  File file = SD.open(webServerFolderPath + LAST_PLAYED_FILE_NAME);

  String fileName;
  while (file.available()) {
    fileName += (char)file.read();
  }
  file.close();
  return fileName;
}

void deleteLastPlayedFile() {
  if (SD.exists(webServerFolderPath + LAST_PLAYED_FILE_NAME)) {
    SD.remove(webServerFolderPath + LAST_PLAYED_FILE_NAME);
  }
}

void deleteIndexFiles() {
  if (SD.exists(webServerFolderPath + "/index.txt")) {
    SD.remove(webServerFolderPath + "/index.txt");
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

// ---------------------------------------------------------------------------
// Presentation helpers
// ---------------------------------------------------------------------------

// Subtitle shown under the box name on every page.
String albumSubtitle() {
  return "Album folder <code>" + htmlEscape(webServerFolderPath) + "</code>";
}

// "/Bibi_Blocksberg_047a8772d42a81" -> "Bibi_Blocksberg" (the token suffix is re-attached by renameFolder)
String folderDisplayName() {
  String name = webServerFolderPath;
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

// ---------------------------------------------------------------------------
// Pages
// ---------------------------------------------------------------------------

void home(AsyncWebServerRequest *request) {
  AsyncResponseStream *res = request->beginResponseStream("text/html");
  pageHead(res, request, "upload", albumSubtitle());

  // Status card
  int percent = batteryPercent();
  const char *pillClass = percent > 50 ? "ok" : percent > 20 ? "warn" : "danger";
  res->print("<div class=\"card\"><div class=\"card-head\"><h2>Status</h2>");
  res->printf("<span class=\"pill %s\"><span class=\"dot\"></span>Battery %d&thinsp;%%</span></div>", pillClass, percent);
  res->print("<dl class=\"kv\">");
  res->printf("<dt>Battery</dt><dd>%d mV &middot; ADC %d</dd>", batteryMilliVolts(), (int)*batteryAdcValueManagementServer);
  res->printf("<dt>Album folder</dt><dd>%s</dd>", htmlEscape(webServerFolderPath).c_str());
  res->printf("<dt>Tracks</dt><dd>%d</dd>", filesInDir.size());
  res->printf("<dt>Free memory</dt><dd>%u kB</dd>", ESP.getFreeHeap() / 1024);
  res->print("</dl></div>");

  // Upload card
  res->print("<div class=\"card\"><div class=\"card-head\"><h2>Upload</h2><span class=\"muted small\">MP3 only</span></div>"
             "<label class=\"dropzone\"><input type=\"file\" id=\"f\" accept=\"audio/mpeg\" multiple>"
             "<strong>Choose MP3 files</strong><span>uploaded one after another to the album folder</span></label>"
             "<div class=\"progress\"><i id=\"bar\"></i></div>"
             "<div class=\"actions between\"><span class=\"hint mono\" id=\"pt\">No files selected</span>"
             "<button class=\"btn\" type=\"button\" id=\"ub\">Upload</button></div></div>");

  // Rename card
  res->print("<div class=\"card\"><div class=\"card-head\"><h2>Rename Folder</h2></div>"
             "<form method=\"POST\" action=\"/setFolderPath\" accept-charset=\"UTF-8\" class=\"field\">"
             "<label for=\"path\">Folder name</label><div class=\"row-inline\">");
  res->printf("<input class=\"input\" id=\"path\" name=\"path\" type=\"text\" value=\"%s\" required spellcheck=\"false\">", htmlEscape(folderDisplayName()).c_str());
  res->print("<button class=\"btn secondary\" type=\"submit\">Rename</button></div>"
             "<p class=\"hint\">The token suffix stays attached automatically. Spaces and umlauts are converted.</p></form></div>");

  // Upload script: one file per request, /temp on the SD card is shared, so uploads must not overlap.
  res->print("<script>(function(){var i=document.getElementById('f'),b=document.getElementById('bar'),t=document.getElementById('pt'),u=document.getElementById('ub'),fs=[],n=0;"
             "i.onchange=function(){fs=i.files;t.textContent=fs.length?fs.length+' file'+(fs.length>1?'s':'')+' selected':'No files selected'};"
             "u.onclick=function(){if(!fs.length){t.textContent='Choose files first';return}n=0;u.disabled=true;i.disabled=true;nx()};"
             "function nx(){if(n>=fs.length){t.textContent='Done';location.href='/';return}var d=new FormData();d.append('files',fs[n]);"
             "t.textContent='Uploading '+(n+1)+' / '+fs.length+' \\u00b7 '+fs[n].name;"
             "fetch('/upload',{method:'POST',body:d}).then(function(r){if(!r.ok)throw new Error('HTTP '+r.status);n++;b.style.width=(n/fs.length*100)+'%';nx()})"
             ".catch(function(e){t.textContent='Upload failed: '+e.message;u.disabled=false;i.disabled=false})}})()</script>");

  pageFoot(res);
  request->send(res);
}

void listFiles(AsyncWebServerRequest *request) {
  AsyncResponseStream *res = request->beginResponseStream("text/html");
  pageHead(res, request, "files", albumSubtitle());

  // Sort the filesInDir alphanumerically
  std::sort(filesInDir.begin(), filesInDir.end());

  int page = 0;
  if (request->hasParam("page")) {
    page = request->getParam("page")->value().toInt();
  }
  int total = filesInDir.size();
  int first = page * PAGE_SIZE;
  int last = std::min(first + PAGE_SIZE, total);

  res->print("<div class=\"card flush\"><div class=\"card-head\"><h2>Files</h2>");
  res->printf("<span class=\"pill neutral mono\">%d track%s</span></div>", total, total == 1 ? "" : "s");
  res->print("<p class=\"muted small lead\">Tap &#9654; to make a track the one that plays next when the token is placed.</p>");

  if (total == 0) {
    res->print("<p class=\"empty\">No tracks yet. Upload some MP3 files.</p>");
  } else {
    res->print("<ul class=\"list\">");
    String lastPlayedFileName = getLastPlayedFileName();
    for (int i = first; i < last; i++) {
      const String &fileName = filesInDir.at(i);
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
  String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
  // Serial.println(logmessage);

  if (!index) {
    logmessage = "Upload Start: " + String(filename);
    // open the file on first call and store the file handle in the request object
    request->_tempFile = SD.open("/temp", "w");
    Serial.println(logmessage);
  }

  if (len) {
    // stream the incoming chunk to the opened file
    request->_tempFile.write(data, len);
    // logmessage = "Writing file: " + String(filename) + " index=" + String(index) + " len=" + String(len);
    // Serial.println(logmessage);
  }

  if (final) {
    logmessage = "Upload Complete: " + String(filename) + ",size: " + String(index + len);
    // close the file handle as the upload is now done
    request->_tempFile.close();
    filename = sanitizeFilename(filename);
    SD.rename("/temp", webServerFolderPath + "/" + filename);
    filesInDir.erase(std::remove(filesInDir.begin(), filesInDir.end(), filename), filesInDir.end());
    filesInDir.push_back(filename);
    deleteIndexFiles();
    Serial.println(logmessage);
  }
}

void handleFileDelete(AsyncWebServerRequest *request) {
  if (request->hasParam("filename", true)) {
    String fileName = request->getParam("filename", true)->value();
    String filePath = webServerFolderPath + "/" + fileName;
    if (SD.exists(filePath)) {
      SD.remove(filePath);
      filesInDir.erase(std::remove(filesInDir.begin(), filesInDir.end(), fileName), filesInDir.end());
      if (getLastPlayedFileName() == filePath) {
        deleteLastPlayedFile();
      }
    }
  }

  deleteIndexFiles();
  request->redirect(request->header("referer"));
}

void handleFileSelect(AsyncWebServerRequest *request) {
  if (request->hasParam("filename", true)) {
    String fileName = request->getParam("filename", true)->value();
    String filePath = webServerFolderPath + "/" + fileName;
    if (SD.exists(filePath)) {
      File file = SD.open(webServerFolderPath + LAST_PLAYED_FILE_NAME, FILE_WRITE);
      if (!file) {
        Serial.println("Failed to open file for writing");
        return;
      }
      if (file.print(filePath)) {
        Serial.println(filePath);
      }
      file.close();
    }
  }
  request->redirect(request->header("referer"));
}

void handleFolderPathChange(AsyncWebServerRequest *request) {
  if (request->hasParam("path", true)) {
    String newFolderPath = request->getParam("path", true)->value();
    if (renameFolder(webServerFolderPath, newFolderPath)) webServerFolderPath = newFolderPath;
  }
  deleteLastPlayedFile();
  deleteIndexFiles();
  request->redirect("/");
}

void prepareFilesInDir() {
  if (!filesInDir.empty()) return;

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

void setupWebServer(String path) {
  webServerFolderPath = path;

  prepareFilesInDir();

  WiFi.softAP(BOX_NAME);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  Serial.println("starting management server");
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", IP);
  server.on("/home", HTTP_GET, home);
  server.on("/", HTTP_GET, listFiles);
  server.on("/css", HTTP_GET, cssHandler);

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) { request->send(200, "text/plain", "File uploaded successfully"); }, handleUpload);
  server.on("/select", HTTP_POST, handleFileSelect);
  server.on("/delete", HTTP_POST, handleFileDelete);
  server.on("/setFolderPath", HTTP_POST, handleFolderPathChange);
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("/home");
  });

  server.begin();
}

#endif
