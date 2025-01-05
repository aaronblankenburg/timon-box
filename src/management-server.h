#ifndef MANAGEMENT_WEB_SERVER
#define MANAGEMENT_WEB_SERVER

#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <SPI.h>

#include <vector>

DNSServer dnsServer;

#define PAGE_SIZE 10

AsyncWebServer server(80);
String webServerFolderPath = "/";
std::vector<String> filesInDir;

void deleteIndexFiles() {
  if (SD.exists(webServerFolderPath + "/index.txt")) {
    SD.remove(webServerFolderPath + "/index.txt");
  }
  if (SD.exists(webServerFolderPath + "/last_played.txt")) {
    SD.remove(webServerFolderPath + "/last_played.txt");
  }
}

bool renameFolder(const String &oldFolder, const String &newFolder) {
  // Make sure the new folder exists (it will be created if not)
  if (!SD.mkdir(newFolder)) {
    Serial.println("Failed to create new folder!");
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

String home() {
  String html = "<h1>Management Server</h1>";
  html += "<h2>Album directory: " + webServerFolderPath + "</h2><br>";

  html += "<form method='POST' action='/setFolderPath'>";
  html += "<input type='text' name='path' value='" + webServerFolderPath + "' required> ";
  html += "<button type='submit'>Rename Folder</button>";
  html += "</form><br>";

  html += "<form id='uploadForm' enctype='multipart/form-data'>";
  html += "<label for='fileInput' class='button button-outline'>Select files</label>";
  html += "<input type='file' id='fileInput' accept='audio/mpeg' multiple>";
  html += "<button type='button' onclick='uploadFiles()'>Upload</button><br><br>";
  html += "<div id='progress'>0 of 0 files uploaded</div><br>";

  html += "<script>let files=[],uploaded=0;function uploadFiles(){const input=document.getElementById('fileInput');files=input.files;uploaded=0;if(files.length===0)return alert('No files selected!');document.getElementById('progress').innerText=`${uploaded} of ${files.length} files uploaded`;uploadNextFile()}function uploadNextFile(){if(uploaded>=files.length){location.reload();return}const formData=new FormData();formData.append('files',files[uploaded]);fetch('/upload',{method:'POST',body:formData}).then(()=>{uploaded++;document.getElementById('progress').innerText=`${uploaded} of ${files.length} files uploaded`;uploadNextFile()}).catch(err=>{alert('Upload failed: '+err)})}</script>";
  html += "<a href='/list'>Files</a>";
  return html;
}

String listFiles(AsyncWebServerRequest *request) {
  String html = "<h1>Management Server</h1>";
  html += "<h2>Album directory: " + webServerFolderPath + "</h2><br>";

  Serial.println("<div>" + String(filesInDir.size()) + " files</div>");
  html += "<div>" + String(filesInDir.size()) + " files</div>";

  // Sort the filesInDir alphanumerically
  std::sort(filesInDir.begin(), filesInDir.end());

  int page = 0;
  if (request->hasParam("page")) {
    page = request->getParam("page")->value().toInt();
  }
  if (page > 0) {
    html += "<a class='button' href='/list?page=" + String(page - 1) + "'>Previous</a> ";
  }
  if (filesInDir.size() > (page + 1) * PAGE_SIZE) {
    html += "<a class='button' href='/list?page=" + String(page + 1) + "'>Next</a>";
  }
  html += "<ul>";
  for (int i = (page * PAGE_SIZE); i < min((page + 1) * PAGE_SIZE, (int)filesInDir.size()); i++) {
    // html += filesInDir.at(i);
    html += "<li><form method='POST' action='/delete'><input type='hidden' name='filename' value='" + filesInDir.at(i) + "'><button type='submit'>x</button> " + filesInDir.at(i) + "</form></li>";
  }
  html += "</ul>";
  return html;
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
  // Serial.println(logmessage);

  if (!index) {
    logmessage = "Upload Start: " + String(filename);
    // open the file on first call and store the file handle in the request object
    request->_tempFile = SD.open(webServerFolderPath + "/" + filename, "w");
    Serial.println(logmessage);
  }

  if (len) {
    // stream the incoming chunk to the opened file
    request->_tempFile.write(data, len);
    logmessage = "Writing file: " + String(filename) + " index=" + String(index) + " len=" + String(len);
    // Serial.println(logmessage);
  }

  if (final) {
    logmessage = "Upload Complete: " + String(filename) + ",size: " + String(index + len);
    // close the file handle as the upload is now done
    request->_tempFile.close();
    deleteIndexFiles();
    Serial.println(logmessage);
    request->send(200, "text/plain", "File uploaded successfully");
  }
}

void handleFileDelete(AsyncWebServerRequest *request) {
  if (request->hasParam("filename", true)) {
    String fileName = request->getParam("filename", true)->value();
    String filePath = webServerFolderPath + "/" + fileName;
    if (SD.exists(filePath)) {
      SD.remove(filePath);
    }
  }

  deleteIndexFiles();
  request->redirect("/");
}

void handleFolderPathChange(AsyncWebServerRequest *request) {
  if (request->hasParam("path", true)) {
    String newFolderPath = request->getParam("path", true)->value();
    if (renameFolder(webServerFolderPath, newFolderPath)) webServerFolderPath = newFolderPath;
  }
  deleteIndexFiles();
  request->redirect("/");
}

void prepareFilesInDir() {
  if (!filesInDir.empty()) return;

  File dir = SD.open(webServerFolderPath);

  while (File file = dir.openNextFile()) {
    esp_task_wdt_reset();
    if (!file.isDirectory()) {
      filesInDir.push_back(String(file.name()));  // Store the filesInDir
    }
    file.close();
  }
  dir.close();
  Serial.println(String(filesInDir.size()) + " files loaded from " + webServerFolderPath);
}

void setupWebServer(String path) {
  webServerFolderPath = path;

  // prepareFilesInDir();

  WiFi.softAP("Timon-Box");
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  Serial.println("starting management server");
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", IP);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", home());
    prepareFilesInDir();
  });
  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request) {
    prepareFilesInDir();
    request->send(200, "text/html", listFiles(request));
  });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) { request->send(200); }, handleUpload);
  server.on("/delete", HTTP_POST, handleFileDelete);
  server.on("/setFolderPath", HTTP_POST, handleFolderPathChange);
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("/");
  });

  server.begin();
}

#endif