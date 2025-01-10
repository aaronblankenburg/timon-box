#ifndef MANAGEMENT_WEB_SERVER
#define MANAGEMENT_WEB_SERVER

#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <SPI.h>

#include <unordered_map>
#include <vector>

#define PAGE_SIZE 20
#define LAST_PLAYED_FILE_NAME "/last_played.txt"

DNSServer dnsServer;

AsyncWebServer server(80);
String webServerFolderPath = "/";
std::vector<String> filesInDir;

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
    filesInDir.erase(std::remove(filesInDir.begin(), filesInDir.end(), "last_played.txt"), filesInDir.end());
  }
}

void deleteIndexFiles() {
  if (SD.exists(webServerFolderPath + "/index.txt")) {
    SD.remove(webServerFolderPath + "/index.txt");
    filesInDir.erase(std::remove(filesInDir.begin(), filesInDir.end(), "index.txt"), filesInDir.end());
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

void css(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = request->beginResponseStream("text/css");
  response->printf("*,::after,::before{box-sizing:border-box}html{font-family:system-ui,'Segoe UI',Roboto,Helvetica,Arial,sans-serif,'Apple Color Emoji','Segoe UI Emoji';line-height:1.15;-webkit-text-size-adjust:100%%;-moz-tab-size:4;tab-size:4}body{margin:0}table{text-indent:0;border-color:inherit}button,input,select{font-family:inherit;font-size:100%%;line-height:1.15;margin:0}button,select{text-transform:none}[type=button],[type=reset],[type=submit],button{-webkit-appearance:button}::-moz-focus-inner{border-style:none;padding:0}:-moz-focusring{outline:1px dotted ButtonText}:-moz-ui-invalid{box-shadow:none}progress{vertical-align:baseline}::-webkit-inner-spin-button,::-webkit-outer-spin-button{height:auto}[type=search]{-webkit-appearance:textfield;outline-offset:-2px}::-webkit-search-decoration{-webkit-appearance:none}::-webkit-file-upload-button{-webkit-appearance:button;font:inherit}[type=button],[type=color],[type=file],[type=range],[type=reset],[type=submit],button,label>[type=checkbox]:enabled,label>[type=radio]:enabled{cursor:pointer}[readonly]{cursor:default}:disabled,[aria-disabled=true],[type=checkbox][id]:disabled+[for],[type=radio][id]:disabled+[for]{cursor:not-allowed}[draggable=true]{cursor:grab}[draggable=true]:active{cursor:grabbing}:root{--max-body-width:48rem}html{height:100%%;line-height:1.4}h1,h2,h3{line-height:1.15}body{display:flex;flex-direction:column;width:calc(100%% - 1rem);max-width:var(--max-body-width);min-height:100%%;margin:0 auto;overflow-wrap:break-word}table{display:block;overflow-x:auto}audio{width:100%%}[type=range],progress{display:block;width:100%%;height:2.25rem}[type=color]{height:2.25rem;vertical-align:top}[type=button],[type=reset],[type=submit],button,td{padding:.5rem}[type=email],[type=hidden],[type=number],[type=password],[type=search],[type=tel],[type=text],[type=url],input:not([type]){display:block;width:100%%;height:2.25rem;padding:.5rem}[type=file]{display:block;width:100%%;height:2.25rem;padding-top:.35rem}[type=date],[type=datetime-local],[type=month],[type=time],[type=week]{display:block;width:100%%;height:2.25rem;padding:.4rem .5rem}select{display:block;width:100%%;min-height:2.25rem;padding:.45rem .5rem}[type=image]{vertical-align:bottom}label>[type=color]{margin-left:.25rem}label{display:flex;flex-wrap:wrap;column-gap:.25rem;align-items:center;width:100%%;margin-top:1rem}[type=checkbox]+label,[type=radio]+label{display:inline-flex;width:unset;vertical-align:text-bottom}label>[type=checkbox],label>[type=radio]{min-height:1rem}[aria-current=date],[aria-current=location],[aria-current=page],[aria-current=step],[aria-current=time],[aria-current=true]{font-weight:700}li{margin-top:.5rem;margin-bottom:.5rem}@media (width <= 50rem){table{width:calc(100%% + 1rem);margin-left:-.5rem}}@media (hover:hover){body{overflow-x:hidden}nav{display:flex;background-color:var(--background-color);border-bottom:var(--border-width) solid var(--border-color)}body>nav{position:relative;left:calc(-50vw + 50%%);width:100vw}nav ul{padding-left:0}body>nav>ul{width:calc(var(--max-body-width) + 2rem);padding-right:.5rem;padding-left:.5rem;margin:.5rem auto}nav ul li{position:relative;display:inline-block}nav>ul>li{padding:.5rem;margin:0}nav ul li a{text-decoration:none;white-space:nowrap}nav ul li ul{position:absolute;left:-9999px;z-index:1;min-width:calc(100%% + var(--border-width) * 2);padding:.25rem .5rem;margin-top:.5rem;margin-left:calc(-.5rem - var(--border-width));background-color:var(--background-color);border:var(--border-width) solid var(--border-color);border-radius:var(--border-radius)}nav ul li ul li{width:100%%}nav ul li ul li ul{min-width:calc(100%% + 1rem + var(--border-width) * 2);margin-top:.75rem}nav :focus~ul,nav :focus~ul ul,nav ul li:hover ul,nav ul ul:focus-within{left:initial}nav li>:not(ul,a,:only-child){cursor:default}nav li>:not(ul,:only-child)::after{content:\" ▾\"}}@media not all and (hover:hover){nav{position:relative;left:calc(-50vw + 50%%);width:100vw;padding-top:2rem;background-image:url(data:image/svg+xml;utf8,SVG_REPLACED);background-repeat:no-repeat;background-position:top;border-bottom:var(--border-width) solid var(--border-color)}nav>*{display:none}nav:hover>ul{display:inherit}nav:hover>:not(ul){display:unset}@media (prefers-color-scheme:dark){nav{background-image:url(data:image/svg+xml;utf8,SVG_REPLACED)}}}:root{color-scheme:light dark;--border-radius:0.25rem;--border-width:1px;--light-border-color:#dbdbdb;--light-border-hover-color:#b5b5b5;--light-background-color:#fff;--light-highlighted-background-color:#f5f5f5;--light-text-color:#363636;--light-danger-color:#f14668;--light-danger-text-color:#fff;--light-danger-hover-color:#f03a5f;--light-success-color:#48c774;--light-success-text-color:#fff;--light-success-hover-color:#3ec46d;--light-danger-text-background-color:#fde0e6;--light-success-text-background-color:#effaf3;--light-code-text-color:#f14668;--light-code-background-color:#f5f5f5;--light-link-color:#3273dc;--light-link-visited-color:#b86bff;--light-link-hover-color:#363636;--light-link-active-color:#363636;--dark-border-color:#5f6267;--dark-border-hover-color:#bcbebd;--dark-background-color:#202124;--dark-highlighted-background-color:#292b2e;--dark-text-color:#fff;--dark-danger-color:#770018;--dark-danger-text-color:#fff;--dark-danger-hover-color:#6b0015;--dark-success-color:#006624;--dark-success-text-color:#fff;--dark-success-hover-color:#006122;--dark-danger-text-background-color:#770018;--dark-success-text-background-color:#006624;--dark-code-text-color:#f1a0b0;--dark-code-background-color:#292b2e;--dark-link-color:#90b3ed;--dark-link-visited-color:#cb93ff;--dark-link-hover-color:#fff;--dark-link-active-color:#fff;--border-color:var(--light-border-color);--border-hover-color:var(--light-border-hover-color);--background-color:var(--light-background-color);--highlighted-background-color:var(--light-highlighted-background-color);--text-color:var(--light-text-color);--danger-color:var(--light-danger-color);--danger-text-color:var(--light-danger-text-color);--danger-hover-color:var(--light-danger-hover-color);--success-color:var(--light-success-color);--success-text-color:var(--light-success-text-color);--success-hover-color:var(--light-success-hover-color);--danger-text-background-color:var(--light-danger-text-background-color);--success-text-background-color:var(--light-success-text-background-color);--code-text-color:var(--light-code-text-color);--code-background-color:var(--light-code-background-color);--link-color:var(--light-link-color);--link-visited-color:var(--light-link-visited-color);--link-hover-color:var(--light-link-hover-color);--link-active-color:var(--light-link-active-color)}@media (prefers-color-scheme:dark){:root{--border-color:var(--dark-border-color);--border-hover-color:var(--dark-border-hover-color);--background-color:var(--dark-background-color);--highlighted-background-color:var(--dark-highlighted-background-color);--text-color:var(--dark-text-color);--danger-color:var(--dark-danger-color);--danger-text-color:var(--dark-danger-text-color);--danger-hover-color:var(--dark-danger-hover-color);--success-color:var(--dark-success-color);--success-text-color:var(--dark-success-text-color);--success-hover-color:var(--dark-success-hover-color);--danger-text-background-color:var(--dark-danger-text-background-color);--success-text-background-color:var(--dark-success-text-background-color);--code-text-color:var(--dark-code-text-color);--code-background-color:var(--dark-code-background-color);--link-color:var(--dark-link-color);--link-visited-color:var(--dark-link-visited-color);--link-hover-color:var(--dark-link-hover-color);--link-active-color:var(--dark-link-active-color)}}input{border-radius:var(--border-radius)}body{color:var(--text-color);background-color:var(--background-color)}a{color:var(--link-color);text-decoration:none}a:visited{color:var(--link-visited-color)}a:hover,a[aria-current=page]{color:var(--link-hover-color)}a:active{color:var(--link-active-color)}button,select{color:var(--text-color);background-color:var(--background-color);border:var(--border-width) solid var(--border-color);border-radius:var(--border-radius)}[type=button],[type=color],[type=date],[type=datetime-local],[type=email],[type=hidden],[type=month],[type=number],[type=password],[type=search],[type=tel],[type=text],[type=time],[type=url],[type=week],input:not([type]){color:var(--text-color);background-color:var(--background-color);border:var(--border-width) solid var(--border-color)}[type=reset]{color:var(--danger-text-color);background-color:var(--danger-color);border:var(--border-width) solid transparent}[type=submit],button:not([type]){color:var(--success-text-color);background-color:var(--success-color);border:var(--border-width) solid transparent}[type=date]:disabled,[type=datetime-local]:disabled,[type=email]:disabled,[type=hidden]:disabled,[type=month]:disabled,[type=number]:disabled,[type=password]:disabled,[type=search]:disabled,[type=tel]:disabled,[type=text]:disabled,[type=time]:disabled,[type=url]:disabled,[type=week]:disabled,input:not([type]):disabled,select:disabled{background-color:var(--highlighted-background-color)}[type=date]:enabled,[type=datetime-local]:enabled,[type=month]:enabled,[type=time]:enabled,[type=week]:enabled,select:enabled{background-color:var(--background-color)}[type=button]:focus,[type=color]:focus,[type=date]:focus,[type=datetime-local]:focus,[type=email]:focus,[type=hidden]:focus,[type=image]:focus,[type=month]:focus,[type=number]:focus,[type=password]:focus,[type=reset]:focus,[type=search]:focus,[type=submit]:focus,[type=tel]:focus,[type=text]:focus,[type=time]:focus,[type=url]:focus,[type=week]:focus,button:focus,button:not([type]):focus,input:not([type]):focus,select:focus{border-color:var(--border-hover-color);outline:0;box-shadow:0 0 .2rem .01rem var(--border-hover-color)}[type=image]:enabled:hover{filter:brightness(95%%)}[type=button]:enabled:hover,[type=color]:enabled:hover,[type=date]:enabled:hover,[type=datetime-local]:enabled:hover,[type=email]:enabled:hover,[type=hidden]:enabled:hover,[type=month]:enabled:hover,[type=number]:enabled:hover,[type=password]:enabled:hover,[type=range]:enabled:hover,[type=search]:enabled:hover,[type=tel]:enabled:hover,[type=text]:enabled:hover,[type=time]:enabled:hover,[type=url]:enabled:hover,[type=week]:enabled:hover,button:enabled:hover,input:not([type]):enabled:hover,select:enabled:hover{border-color:var(--border-hover-color)}[type=reset]:enabled:hover{background-color:var(--danger-hover-color);border-color:transparent}[type=submit]:enabled:hover,button:not([type]):enabled:hover{background-color:var(--success-hover-color);border-color:transparent}table{border-collapse:collapse}tbody>tr:not(:first-child){border-top:var(--border-width) solid var(--border-color)}tbody>tr:nth-child(2n){background-color:var(--highlighted-background-color)}:disabled,[aria-disabled=true],[type=checkbox][id]:disabled+[for],[type=radio][id]:disabled+[for]{opacity:.5}:invalid:not(form){border-color:var(--danger-color);border-style:solid;border-width:var(--border-width)}:disabled :disabled,[aria-disabled=true] [aria-disabled=true]{opacity:unset}:invalid:not([type=checkbox],:focus){box-shadow:none}:invalid:not([type=checkbox]):focus{border-color:var(--danger-hover-color);outline:0;box-shadow:0 0 .2rem .05rem var(--danger-hover-color)}:invalid:not(form):enabled:hover{border-color:var(--danger-hover-color)}");

  request->send(response);
}

void home(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = request->beginResponseStream("text/html");

  response->printf("<link rel='stylesheet' href='/css' />");
  response->printf("<h1>Timon-Box</h1>");
  response->printf("<h2>Album directory: %s</h2><br>", webServerFolderPath.c_str());

  response->printf("<nav><ul><li><a href='/'>Files</a></li></ul></nav>");

  response->printf("<h3>Rename Folder</h3>");

  response->printf("<form method='POST' action='/setFolderPath' accept-charset='UTF-8'>");
  response->printf("<input type='text' name='path' value='%s' required> ", webServerFolderPath.c_str());
  response->printf("<button type='submit'>Rename Folder</button>");
  response->printf("</form><br>");

  response->printf("<h3>File Upload</h3>");

  response->printf("<form id='uploadForm' enctype='multipart/form-data'>");
  response->printf("<input type='file' id='fileInput' accept='audio/mpeg' multiple>");
  response->printf("<button type='button' onclick='uploadFiles()'>Upload</button><br><br>");
  response->printf("<progress id='progress' value='0' max='0'></progress>");

  response->printf("<script>");
  response->printf("let files=[],uploaded=0;function uploadFiles(){const input=document.getElementById('fileInput');files=input.files;uploaded=0;if(files.length===0)return alert('No files selected!');document.getElementById('progress').removeAttribute('value');document.getElementById('progress').max=files.length;uploadNextFile()}function uploadNextFile(){if(uploaded>=files.length){location.reload();return}const formData=new FormData();formData.append('files',files[uploaded]);fetch('/upload',{method:'POST',body:formData}).then(()=>{uploaded++;document.getElementById('progress').value=uploaded;document.getElementById('progress').max=files.length;uploadNextFile()}).catch(err=>{alert('Upload failed: '+err)})}");
  response->printf("</script>");

  request->send(response);
}

void listFiles(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = request->beginResponseStream("text/html");

  response->printf("<link rel='stylesheet' href='/css' />");
  response->printf("<h1>Timon-Box</h1>");
  response->printf("<h2>Album directory: %s</h2><br>", webServerFolderPath.c_str());

  // Sort the filesInDir alphanumerically
  std::sort(filesInDir.begin(), filesInDir.end());

  int page = 0;
  if (request->hasParam("page")) {
    page = request->getParam("page")->value().toInt();
  }
  response->printf("<nav><ul><li><a href='/home'>Upload</a></li>");

  if (page > 0) {
    response->printf("<li><a class='button' href='/list?page=%d'>Previous</a></li> ", page - 1);
  }
  if (filesInDir.size() > (page + 1) * PAGE_SIZE) {
    response->printf("<li><a class='button' href='/list?page=%d'>Next</a></li>", page + 1);
  }
  response->printf("</ul></nav>");
  response->printf("<h3>File Management</h3>");

  response->printf("<table><caption>%d files</caption><thead><tr><th>Playing</th><th>Name</th><th>Delete</th></tr></thead><tbody>", filesInDir.size());
  String lastPlayedFileName = getLastPlayedFileName();
  for (int i = (page * PAGE_SIZE); i < std::min((page + 1) * PAGE_SIZE, (int)filesInDir.size()); i++) {
    String fileName = filesInDir.at(i);
    String selectButtonLabel = lastPlayedFileName.endsWith(fileName) ? "&#x25B6;" : "&#x25B7;";
    response->printf("<tr><td><form method='POST' action='/select' style='margin:0'><input type='hidden' name='filename' value='%s'><button type='submit'>%s</button></form></td><td>%s</td><td style='text-align:right'><form method='POST' action='/delete' style='margin:0'><input type='hidden' name='filename' value='%s'><button type='submit' style='background-color:red'>&#x274C;</button></form></td></tr>", fileName.c_str(), selectButtonLabel, fileName.c_str(), fileName.c_str());
  }
  response->printf("</tbody></table>");

  request->send(response);
}

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
    // close the file handle as the upload is now doneu
    request->_tempFile.close();
    filename = sanitizeFilename(filename);
    SD.rename("/temp", webServerFolderPath + "/" + filename);
    filesInDir.erase(std::remove(filesInDir.begin(), filesInDir.end(), filename), filesInDir.end());
    filesInDir.push_back(filename);
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

  prepareFilesInDir();

  WiFi.softAP("Timon-Box");
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  Serial.println("starting management server");
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", IP);
  server.on("/home", HTTP_GET, home);
  server.on("/", HTTP_GET, listFiles);
  server.on("/css", HTTP_GET, css);

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) { request->send(200); }, handleUpload);
  server.on("/select", HTTP_POST, handleFileSelect);
  server.on("/delete", HTTP_POST, handleFileDelete);
  server.on("/setFolderPath", HTTP_POST, handleFolderPathChange);
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("/");
  });

  server.begin();
}

#endif