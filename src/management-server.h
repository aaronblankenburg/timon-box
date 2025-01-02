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

String listFiles() {
  String html = "<html><head><link rel=\"stylesheet\" href=\"https://unpkg.com/plain-css@latest/dist/plain.min.css\"></head><body><form method='POST' enctype='multipart/form-data' action='/upload'>";
  html += "<input type='file' name='files' multiple><input type='submit'></form><ul>";
  File dir = SD.open(webServerFolderPath);
  while (File file = dir.openNextFile()) {
    if (!file.isDirectory()) {
      html += "<li><a href='/delete?file=" + String(file.name()) + "' class=\"pl-button full red\">Delete</a> ";
      html += file.name();
      html += "</li>";
    }
    file.close();
  }
  dir.close();
  html += "</ul></body></html>";
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
    request->redirect("/");
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

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", listFiles());
  });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) { request->send(200); }, handleUpload);

  server.on("/delete", HTTP_GET, handleFileDelete);
  server.begin();
}

#endif