#ifndef MANAGEMENT_WEB_SERVER
#define MANAGEMENT_WEB_SERVER

#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <SPI.h>

AsyncWebServer server(80);
String webServerFolderPath = "/";

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

String listFiles() {
  String html = "<style>body{font-family:'Roboto','Helvetica Neue','Helvetica','Arial',sans-serif;font-size:1.6em;font-weight:300;line-height:1.6}input[type='file'],button{margin-bottom:1rem;font-size:1rem;padding:0.5rem 1rem}button{background-color:#9b4dca;color:#fff;border:none;border-radius:0.4rem;cursor:pointer;text-transform:uppercase}button:hover{background-color:#606c76}div#progress{font-size:1rem;margin-top:0.5rem}a{color:#9b4dca;text-decoration:none}a:hover{color:#606c76}.button{background-color:#ff4d4d;color:#fff;padding:0.3rem 0.6rem;border-radius:0.4rem;text-decoration:none;margin-right:0.5rem;}.button-outline{border:1px solid #9b4dca;background-color:transparent;color:#9b4dca;padding:0.3rem 0.6rem;border-radius:0.4rem;cursor:pointer;text-transform:uppercase}input[type='file']{display:none}input[type='text']{-webkit-appearance:none;background-color:transparent;border:0.1rem solid #d1d1d1;border-radius:.4rem;box-shadow:none;box-sizing:inherit;height:3.8rem;padding:.6rem 1.0rem .7rem;width:100%;}</style>";
  html += "<h1>Management Server</h1>";
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

  html += "<ul>";
  File dir = SD.open(webServerFolderPath);
  while (File file = dir.openNextFile()) {
    if (!file.isDirectory()) {
      html += "<li><a href='/delete?file=" + String(file.name()) + "' class='button'>Delete</a> " + file.name() + "</li>";
    }
    file.close();
  }
  html += "</ul>";
  dir.close();
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
  if (request->hasParam("file")) {
    String fileName = request->getParam("file")->value();
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
  request->redirect("/");
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", listFiles());
  });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) { request->send(200); }, handleUpload);
  server.on("/delete", HTTP_GET, handleFileDelete);
  server.on("/setFolderPath", HTTP_POST, handleFolderPathChange);
  server.begin();
}

#endif