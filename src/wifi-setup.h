#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H

// Network layer of the management server.
//
// With a home network saved in NVS the box joins it as a station and announces
// itself via mDNS as <box-name>.local. Without saved credentials, or when the
// network cannot be reached within WIFI_CONNECT_TIMEOUT_MS, it falls back to
// the old behaviour: an open hotspot named BOX_NAME with a captive-portal DNS.
//
// The /wifi page lets the user pick a network, save the password, test the
// connection (the hotspot stays up meanwhile) and forget the network again.
//
// Threading: web handlers run on the AsyncTCP task and only queue actions or
// read state; every WiFi/mDNS/DNS/NVS mutation happens in netLoop() on the
// Arduino loop task.

#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <algorithm>
#include <vector>

#include "web-ui.h"

#ifndef WIFI_CONNECT_TIMEOUT_MS
#define WIFI_CONNECT_TIMEOUT_MS 15000
#endif
#define STA_LOST_TIMEOUT_MS 30000  // station link gone for this long -> hotspot
#define NET_ACTION_GRACE_MS 400    // let the HTTP response reach the browser before touching the radio

enum NetMode { NET_CONNECTING,
               NET_STATION,
               NET_ACCESS_POINT };
enum ConnectResult { CR_NONE,
                     CR_OK,
                     CR_FAILED };
enum PendingActionType { PA_NONE,
                         PA_CONNECT,
                         PA_FORGET };

struct WifiNetwork {
  String ssid;
  int32_t rssi;
  bool secure;
};

struct PendingAction {
  PendingActionType type = PA_NONE;
  String ssid;
  String pass;
  unsigned long at = 0;
};

DNSServer dnsServer;
Preferences wifiPrefs;

volatile NetMode netMode = NET_ACCESS_POINT;
volatile ConnectResult connectResult = CR_NONE;
volatile uint8_t lastDisconnectReason = 0;  // written by the WiFi event task
volatile bool scanRequested = false;
volatile bool scanInProgress = false;
volatile bool scanDone = false;
volatile bool scanFailed = false;
bool apActive = false;  // loop task only
unsigned long connectStart = 0;
unsigned long staLostSince = 0;

SemaphoreHandle_t netMutex = nullptr;  // guards pendingAction, scanResults, savedSsidCache
PendingAction pendingAction;
std::vector<WifiNetwork> scanResults;
String savedSsidCache;

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

// "Timon-Box" -> "timon-box"; used for mDNS and the DHCP hostname.
const char *boxHostname() {
  static String hostname = [] {
    String s = BOX_NAME;
    s.toLowerCase();
    return s;
  }();
  return hostname.c_str();
}

bool netLock(uint32_t waitMs = 100) {
  return netMutex && xSemaphoreTake(netMutex, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}

void netUnlock() { xSemaphoreGive(netMutex); }

String getSavedSsid() {
  String copy;
  if (netLock()) {
    copy = savedSsidCache;
    netUnlock();
  }
  return copy;
}

void setSavedSsid(const String &ssid) {
  if (netLock()) {
    savedSsidCache = ssid;
    netUnlock();
  }
}

bool loadCredentials(String &ssid, String &pass) {
  ssid = wifiPrefs.getString("ssid", "");
  pass = wifiPrefs.getString("pass", "");
  return ssid.length() > 0;
}

void saveCredentials(const String &ssid, const String &pass) {
  wifiPrefs.putString("ssid", ssid);
  wifiPrefs.putString("pass", pass);
  setSavedSsid(ssid);
}

void clearCredentials() {
  wifiPrefs.remove("ssid");
  wifiPrefs.remove("pass");
  setSavedSsid("");
}

String jsonEscape(const String &in) {
  String out;
  out.reserve(in.length() + 8);
  for (unsigned int i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if ((uint8_t)c < 0x20) {
      char buf[7];
      snprintf(buf, sizeof(buf), "\\u%04x", c);
      out += buf;
    } else {
      out += c;
    }
  }
  return out;
}

const char *disconnectHint(uint8_t reason) {
  switch (reason) {
    case WIFI_REASON_NO_AP_FOUND:
      return "Network not found. Check the name and that the router is in range.";
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_CONNECTION_FAIL:
      return "Authentication failed. Wrong password?";
    case 0:
      return "No answer from the network within the time limit.";
    default:
      return "The connection could not be established.";
  }
}

// ---------------------------------------------------------------------------
// Radio state changes (loop task only)
// ---------------------------------------------------------------------------

void startConnect(const String &ssid, const String &pass) {
  Serial.printf("[net] connecting to \"%s\"%s\n", ssid.c_str(), apActive ? " (hotspot stays up)" : "");
  WiFi.setAutoReconnect(false);  // we own the attempt; the core would otherwise retry forever and hop channels
  lastDisconnectReason = 0;
  connectResult = CR_NONE;
  WiFi.mode(apActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.length() ? pass.c_str() : nullptr);
  connectStart = millis();
  netMode = NET_CONNECTING;
}

void enterAccessPoint() {
  WiFi.setAutoReconnect(false);
  if (WiFi.getMode() & WIFI_MODE_STA) WiFi.disconnect(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(BOX_NAME);
  IPAddress ip = WiFi.softAPIP();
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", ip);
  apActive = true;
  netMode = NET_ACCESS_POINT;
  Serial.printf("[net] hotspot \"%s\" at %s\n", BOX_NAME, ip.toString().c_str());
}

void onStaDisconnected(arduino_event_id_t, arduino_event_info_t info) {
  lastDisconnectReason = info.wifi_sta_disconnected.reason;
}

// Call once from setupWebServer(), before server.begin().
void netBegin() {
  netMutex = xSemaphoreCreateMutex();
  wifiPrefs.begin("wifi", false);

  WiFi.persistent(false);  // credentials live in our own NVS namespace, not in the driver's
  WiFi.setAutoReconnect(false);
  WiFi.setSleep(false);  // modem sleep adds 100-300 ms latency per request and slows uploads
  WiFi.setHostname(boxHostname());
  WiFi.onEvent(onStaDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  String ssid, pass;
  if (loadCredentials(ssid, pass)) {
    setSavedSsid(ssid);
    startConnect(ssid, pass);
  } else {
    Serial.println("[net] no saved network");
    enterAccessPoint();
  }

  // Once per boot is enough: the IDF responder follows AP/STA interface changes itself.
  if (MDNS.begin(boxHostname())) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[net] mDNS %s.local\n", boxHostname());
  } else {
    Serial.println("[net] mDNS start failed");
  }
  Serial.printf("[net] free heap %u\n", ESP.getFreeHeap());
}

void handlePendingAction() {
  PendingAction action;
  if (!netLock()) return;
  if (pendingAction.type != PA_NONE && millis() - pendingAction.at >= NET_ACTION_GRACE_MS) {
    action = pendingAction;
    pendingAction = PendingAction();
  }
  netUnlock();

  if (action.type == PA_CONNECT) {
    // Saved before the test on purpose: a network that is out of range right now
    // can still be provisioned, and a wrong password only costs the 15 s fallback.
    saveCredentials(action.ssid, action.pass);
    startConnect(action.ssid, action.pass);
  } else if (action.type == PA_FORGET) {
    Serial.println("[net] saved network forgotten");
    clearCredentials();
    connectResult = CR_NONE;
    if (apActive) {
      if (WiFi.getMode() & WIFI_MODE_STA) WiFi.disconnect(false);
      netMode = NET_ACCESS_POINT;
    } else {
      enterAccessPoint();
    }
  }
}

void handleScan() {
  if (scanInProgress) {
    int16_t n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;

    std::vector<WifiNetwork> found;
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.isEmpty()) continue;
      int32_t rssi = WiFi.RSSI(i);
      bool secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      auto same = std::find_if(found.begin(), found.end(), [&](const WifiNetwork &w) { return w.ssid == ssid; });
      if (same == found.end()) {
        found.push_back({ssid, rssi, secure});
      } else if (rssi > same->rssi) {
        same->rssi = rssi;
        same->secure = secure;
      }
    }
    std::sort(found.begin(), found.end(), [](const WifiNetwork &a, const WifiNetwork &b) { return a.rssi > b.rssi; });
    WiFi.scanDelete();  // results buffer belongs to the core; we copied what we need

    if (netLock()) {
      scanResults = found;
      netUnlock();
    }
    scanFailed = n < 0;
    scanDone = true;
    scanInProgress = false;
    Serial.printf("[net] scan finished: %d networks\n", (int)found.size());
    return;
  }

  if (scanRequested && netMode != NET_CONNECTING) {  // the driver refuses scans while associating
    scanRequested = false;
    scanDone = false;
    scanFailed = false;
    scanInProgress = true;
    if (WiFi.scanNetworks(true, false) == WIFI_SCAN_FAILED) {
      scanInProgress = false;
      scanFailed = true;
      scanDone = true;
      Serial.println("[net] scan could not be started");
    } else {
      Serial.println("[net] scan started");
    }
  }
}

// Drive the network state machine. Call every loop() iteration while the management server runs.
NetMode netLoop() {
  handlePendingAction();
  handleScan();

  switch (netMode) {
    case NET_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        netMode = NET_STATION;
        connectResult = CR_OK;
        staLostSince = 0;
        WiFi.setAutoReconnect(true);
        Serial.printf("[net] connected to \"%s\" ip=%s rssi=%d after %lu ms, reachable as %s.local, free heap %u\n",
                      WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI(),
                      millis() - connectStart, boxHostname(), ESP.getFreeHeap());
      } else if (millis() - connectStart > WIFI_CONNECT_TIMEOUT_MS) {
        connectResult = CR_FAILED;
        Serial.printf("[net] connect timeout after %d ms, last disconnect reason %u\n", WIFI_CONNECT_TIMEOUT_MS, lastDisconnectReason);
        if (apActive) {
          WiFi.disconnect(false);  // test from the portal failed, the hotspot is still up
          netMode = NET_ACCESS_POINT;
        } else {
          enterAccessPoint();
        }
      }
      break;

    case NET_STATION:
      if (WiFi.status() != WL_CONNECTED) {
        if (!staLostSince) {
          staLostSince = millis();
          Serial.println("[net] station link lost, waiting for reconnect");
        } else if (millis() - staLostSince > STA_LOST_TIMEOUT_MS) {
          Serial.println("[net] link did not come back, falling back to hotspot");
          if (apActive) {
            netMode = NET_ACCESS_POINT;
          } else {
            enterAccessPoint();
          }
        }
      } else {
        staLostSince = 0;
      }
      break;

    case NET_ACCESS_POINT:
      break;
  }

  if (apActive) dnsServer.processNextRequest();  // captive DNS only while the hotspot is up
  return netMode;
}

// ---------------------------------------------------------------------------
// Web handlers (AsyncTCP task)
// ---------------------------------------------------------------------------

void wifiStatusJson(AsyncWebServerRequest *request) {
  NetMode mode = netMode;
  ConnectResult result = connectResult;
  uint8_t reason = lastDisconnectReason;
  bool connected = WiFi.status() == WL_CONNECTED;

  String json = "{\"mode\":\"";
  json += mode == NET_CONNECTING ? "connecting" : mode == NET_STATION ? "station" : "ap";
  json += "\",\"connected\":";
  json += connected ? "true" : "false";
  json += ",\"ssid\":\"" + jsonEscape(connected ? WiFi.SSID() : String("")) + "\"";
  json += ",\"ip\":\"" + (connected ? WiFi.localIP().toString() : String("")) + "\"";
  json += ",\"rssi\":" + String(connected ? WiFi.RSSI() : 0);
  json += ",\"hostname\":\"" + String(boxHostname()) + ".local\"";
  json += ",\"apActive\":";
  json += apActive ? "true" : "false";
  json += ",\"apSsid\":\"" BOX_NAME "\"";
  json += ",\"apIp\":\"" + (apActive ? WiFi.softAPIP().toString() : String("")) + "\"";
  json += ",\"result\":\"";
  json += result == CR_NONE ? "none" : result == CR_OK ? "ok" : "failed";
  json += "\",\"reason\":" + String(reason);
  json += ",\"hint\":\"" + jsonEscape(result == CR_FAILED ? disconnectHint(reason) : "") + "\"";
  json += ",\"saved\":\"" + jsonEscape(getSavedSsid()) + "\"";
  json += ",\"heap\":" + String(ESP.getFreeHeap()) + "}";
  request->send(200, "application/json", json);
}

void wifiScanJson(AsyncWebServerRequest *request) {
  if (request->hasParam("rescan") || (!scanDone && !scanInProgress)) scanRequested = true;

  const char *state = "scanning";
  if (netMode == NET_CONNECTING) {
    state = "busy";
  } else if (!scanRequested && !scanInProgress && scanDone) {
    state = scanFailed ? "failed" : "done";
  }

  String json = String("{\"state\":\"") + state + "\",\"networks\":[";
  if (netLock()) {
    for (size_t i = 0; i < scanResults.size(); i++) {
      if (i) json += ',';
      json += "{\"ssid\":\"" + jsonEscape(scanResults[i].ssid) + "\",\"rssi\":" + String(scanResults[i].rssi) + ",\"secure\":" + (scanResults[i].secure ? "true" : "false") + "}";
    }
    netUnlock();
  }
  json += "]}";
  request->send(200, "application/json", json);
}

void wifiConnectPost(AsyncWebServerRequest *request) {
  if (!request->hasParam("ssid", true)) {
    request->redirect("/wifi?err=1");
    return;
  }
  String ssid = request->getParam("ssid", true)->value();
  String pass = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : "";
  ssid.trim();
  if (ssid.isEmpty() || ssid.length() > 32 || (pass.length() > 0 && (pass.length() < 8 || pass.length() > 63))) {
    request->redirect("/wifi?err=1");
    return;
  }

  bool queued = false;
  if (netLock()) {
    pendingAction.type = PA_CONNECT;
    pendingAction.ssid = ssid;
    pendingAction.pass = pass;
    pendingAction.at = millis();
    queued = true;
    netUnlock();
  }
  if (!queued) {
    request->redirect("/wifi?err=2");
    return;
  }
  connectResult = CR_NONE;  // so the status poll does not report a stale result
  request->redirect("/wifi?connecting=1");
}

void wifiForgetPost(AsyncWebServerRequest *request) {
  if (netLock()) {
    pendingAction.type = PA_FORGET;
    pendingAction.ssid = "";
    pendingAction.pass = "";
    pendingAction.at = millis();
    netUnlock();
  }
  request->redirect("/wifi?forgot=1");
}

void wifiPage(AsyncWebServerRequest *request) {
  NetMode mode = netMode;
  bool connected = WiFi.status() == WL_CONNECTED;
  bool station = mode == NET_STATION && connected;
  String saved = getSavedSsid();
  String hostname = String(boxHostname()) + ".local";

  AsyncResponseStream *res = request->beginResponseStream("text/html");
  pageHead(res, request, "wifi");

  // Connection card
  res->print("<div class=\"card\"><div class=\"card-head\"><h2>Connection</h2>");
  if (station) {
    res->print("<span class=\"pill ok\" id=\"mp\"><span class=\"dot\"></span>Home network</span>");
  } else if (mode == NET_CONNECTING) {
    res->print("<span class=\"pill accent\" id=\"mp\"><span class=\"dot\"></span>Connecting</span>");
  } else {
    res->print("<span class=\"pill warn\" id=\"mp\"><span class=\"dot\"></span>Hotspot</span>");
  }
  res->print("</div><dl class=\"kv\">");
  res->print("<dt>Mode</dt><dd id=\"k-mode\">");
  if (station) {
    res->print(apActive ? "Home network <span class=\"muted\">(hotspot still up)</span>" : "Home network");
  } else if (mode == NET_CONNECTING) {
    res->print("Connecting&hellip;");
  } else {
    res->print("Own hotspot <span class=\"muted\">(fallback)</span>");
  }
  res->print("</dd><dt>Network</dt><dd id=\"k-net\">");
  if (station) {
    res->printf("%s &middot; %d dBm", htmlEscape(WiFi.SSID()).c_str(), WiFi.RSSI());
  } else {
    res->print(BOX_NAME);
  }
  res->print("</dd><dt>Address</dt><dd id=\"k-ip\">");
  if (station) res->print(WiFi.localIP().toString());
  if (station && apActive) res->print(" &middot; hotspot ");
  if (apActive) res->print(WiFi.softAPIP().toString());
  res->printf("</dd><dt>Hostname</dt><dd>%s</dd>", hostname.c_str());
  res->printf("<dt>Saved network</dt><dd id=\"k-saved\">%s</dd></dl>", saved.isEmpty() ? "<span class=\"muted\">none</span>" : htmlEscape(saved).c_str());
  res->printf("<p class=\"hint\">With a saved home network the box joins it on the next management session and is reachable as <span class=\"mono\">%s</span>. "
              "If that network is out of reach for %d seconds, it opens the hotspot &ldquo;%s&rdquo; instead.</p></div>",
              hostname.c_str(), WIFI_CONNECT_TIMEOUT_MS / 1000, BOX_NAME);

  res->print("<div id=\"bn\"></div>");

  // Join card
  res->print("<div class=\"card\"><div class=\"card-head\"><h2>Join a Home Network</h2>"
             "<button class=\"btn secondary small\" type=\"button\" id=\"rs\">Rescan</button></div>"
             "<ul class=\"nets\" id=\"nl\" role=\"listbox\" aria-label=\"Nearby networks\"><li class=\"status\">Scanning&hellip;</li></ul>"
             "<form method=\"POST\" action=\"/wifi-connect\" accept-charset=\"UTF-8\" style=\"display:flex;flex-direction:column;gap:16px\">"
             "<div class=\"field\"><label for=\"ssid\">Network</label>");
  res->printf("<input class=\"input\" id=\"ssid\" name=\"ssid\" type=\"text\" value=\"%s\" maxlength=\"32\" required autocomplete=\"off\" spellcheck=\"false\"></div>", htmlEscape(saved).c_str());
  res->print("<div class=\"field\"><label for=\"pass\">Password</label>"
             "<input class=\"input\" id=\"pass\" name=\"pass\" type=\"password\" maxlength=\"63\" placeholder=\"8&ndash;63 characters, empty for open networks\" autocomplete=\"off\"></div>"
             "<div class=\"actions between\"><span class=\"hint\" id=\"jh\"></span>"
             "<button class=\"btn\" type=\"submit\">Connect &amp; Save</button></div></form></div>");

  // Forget card
  res->print("<div class=\"card\"><div class=\"card-head\"><h2>Forget Network</h2></div>"
             "<form method=\"POST\" action=\"/wifi-forget\" class=\"actions between\" onsubmit=\"return confirm('Forget the saved network?')\">");
  if (saved.isEmpty()) {
    res->print("<p class=\"hint\">Nothing saved yet. Once a network is saved, this removes it and the box opens its own hotspot again.</p>"
               "<button class=\"btn destructive\" type=\"submit\" disabled>Forget</button>");
  } else {
    res->printf("<p class=\"hint\">Removes &ldquo;%s&rdquo;. The box opens its own hotspot again.</p>"
                "<button class=\"btn destructive\" type=\"submit\">Forget</button>",
                htmlEscape(saved).c_str());
  }
  res->print("</form></div>");

  // Page script: network list from /wifi-scan, connection result from /wifi-status.
  res->print("<script>(function(){var q=location.search,nl=document.getElementById('nl'),si=document.getElementById('ssid'),bn=document.getElementById('bn'),jh=document.getElementById('jh'),mp=document.getElementById('mp');"
             "function esc(s){return String(s).replace(/[&<>\"']/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',\"'\":'&#39;'}[c]})}"
             "function lv(r){return r>-55?4:r>-65?3:r>-75?2:1}"
             "function banner(c,h){bn.innerHTML=h?'<div class=\"banner '+c+'\">'+h+'</div>':''}"
             "function render(d){if(d.state==='done'&&d.networks.length){nl.innerHTML=d.networks.map(function(n){return '<li role=\"option\" data-ssid=\"'+esc(n.ssid)+'\" aria-selected=\"'+(n.ssid===si.value)+'\"><i class=\"ic ic-check\"></i><span class=\"ssid\">'+esc(n.ssid)+'</span><i class=\"ic ic-lock'+(n.secure?'':' open')+'\"></i><span class=\"bars\" data-level=\"'+lv(n.rssi)+'\"><i></i><i></i><i></i><i></i></span></li>'}).join('')}"
             "else if(d.state==='done')nl.innerHTML='<li class=\"status\">No networks found</li>';"
             "else if(d.state==='failed')nl.innerHTML='<li class=\"status\">Scan failed. Try again.</li>';"
             "else if(d.state==='busy')nl.innerHTML='<li class=\"status\">Waiting for the connection test to finish\\u2026</li>';"
             "else nl.innerHTML='<li class=\"status\">Scanning\\u2026</li>'}"
             "var st;function scan(re){clearTimeout(st);fetch('/wifi-scan'+(re?'?rescan=1':'')).then(function(r){return r.json()}).then(function(d){render(d);if(d.state!=='done'&&d.state!=='failed')st=setTimeout(function(){scan(false)},1500)}).catch(function(){st=setTimeout(function(){scan(false)},2000)})}"
             "nl.onclick=function(e){var li=e.target.closest('li[data-ssid]');if(!li)return;nl.querySelectorAll('li').forEach(function(x){x.setAttribute('aria-selected','false')});li.setAttribute('aria-selected','true');si.value=li.dataset.ssid;document.getElementById('pass').focus()};"
             "document.getElementById('rs').onclick=function(){scan(true)};scan(false);");
  res->print("jh.textContent='");
  if (station) {
    res->printf("Switching networks disconnects this page. Reach the box again via %s.local or the hotspot.", boxHostname());
  } else {
    res->print("The hotspot stays up while testing. The network is saved even if the test fails.");
  }
  res->print("';");
  res->print("function poll(){fetch('/wifi-status').then(function(r){return r.json()}).then(function(d){"
             "if(d.result==='ok'){banner('ok','<span>&#10003;</span><span><b>Connected</b> to '+esc(d.ssid)+' as <span class=\"mono\">'+d.ip+'</span> \\u00b7 reachable at <span class=\"mono\">'+d.hostname+'</span>. The next management session joins this network directly.</span>');"
             "mp.className='pill ok';mp.innerHTML='<span class=\"dot\"></span>Home network';document.getElementById('k-mode').innerHTML='Home network'+(d.apActive?' <span class=\"muted\">(hotspot still up)</span>':'');document.getElementById('k-net').textContent=d.ssid+' \\u00b7 '+d.rssi+' dBm';document.getElementById('k-ip').textContent=d.ip+(d.apActive?' \\u00b7 hotspot '+d.apIp:'');document.getElementById('k-saved').textContent=d.saved}"
             "else if(d.result==='failed'){banner('fail','<span>&#10007;</span><span><b>Could not connect.</b> '+esc(d.hint)+' The network is saved anyway; correct the password and try again.</span>');document.getElementById('k-saved').textContent=d.saved}"
             "else setTimeout(poll,1000)}).catch(function(){setTimeout(poll,1500)})}"
             "if(/connecting=1/.test(q)){banner('','<span class=\"spinner\"></span><span>Connecting\\u2026 this page keeps checking.</span>');poll()}"
             "if(/forgot=1/.test(q))banner('','<span>Saved network removed. The box runs as its own hotspot again.</span>');"
             "if(/err=1/.test(q))banner('fail','<span>Please enter a network name (1\\u201332 characters) and a password of 8\\u201363 characters, or none for an open network.</span>');"
             "if(/err=2/.test(q))banner('fail','<span>The box is busy, please try again.</span>');"
             "})()</script>");

  pageFoot(res);
  request->send(res);
}

void registerWifiRoutes(AsyncWebServer &server) {
  server.on("/wifi", HTTP_GET, wifiPage);
  server.on("/wifi-scan", HTTP_GET, wifiScanJson);
  server.on("/wifi-status", HTTP_GET, wifiStatusJson);
  server.on("/wifi-connect", HTTP_POST, wifiConnectPost);
  server.on("/wifi-forget", HTTP_POST, wifiForgetPost);
}

#endif
