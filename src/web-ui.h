#ifndef WEB_UI_H
#define WEB_UI_H

// Shared page chrome for the management web UI: stylesheet, <head>, header with
// title + appearance toggle, segmented navigation and the closing footer.
// Styling follows Apple's Human Interface Guidelines (system font, #f5f5f7 ground,
// 18px cards, pill buttons, #0071e3 accent) in a light and a dark variant.

#include <ESPAsyncWebServer.h>

// ---------------------------------------------------------------------------
// Stylesheet (raw string literal: no printf escaping needed, sent uncopied)
// ---------------------------------------------------------------------------
static const char PAGE_CSS[] = R"CSS(:root{color-scheme:light dark;--bg:#f5f5f7;--card:#fff;--text:#1d1d1f;--text2:#6e6e73;--text3:#86868b;--accent:#0071e3;--accent-tint:rgba(0,113,227,.12);--accent-ring:rgba(0,113,227,.3);--danger:#ff3b30;--danger-tint:rgba(255,59,48,.12);--ok:#34c759;--ok-tint:rgba(52,199,89,.14);--warn:#ff9f0a;--warn-tint:rgba(255,159,10,.16);--sep:rgba(0,0,0,.08);--seg:#e8e8ed;--seg-active:#fff;--field:#fff;--row-hover:rgba(0,0,0,.03);--shadow:0 2px 12px rgba(0,0,0,.06);--shadow-seg:0 1px 3px rgba(0,0,0,.12)}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){--bg:#000;--card:#1c1c1e;--text:#f5f5f7;--text2:#86868b;--text3:#6e6e73;--accent:#0a84ff;--accent-tint:rgba(10,132,255,.18);--accent-ring:rgba(10,132,255,.4);--danger:#ff453a;--danger-tint:rgba(255,69,58,.18);--ok:#30d158;--ok-tint:rgba(48,209,88,.18);--warn:#ffd60a;--warn-tint:rgba(255,214,10,.18);--sep:rgba(255,255,255,.12);--seg:#2c2c2e;--seg-active:#636366;--field:#2c2c2e;--row-hover:rgba(255,255,255,.04);--shadow:0 2px 12px rgba(0,0,0,.4);--shadow-seg:0 1px 3px rgba(0,0,0,.5)}}
:root[data-theme="dark"]{--bg:#000;--card:#1c1c1e;--text:#f5f5f7;--text2:#86868b;--text3:#6e6e73;--accent:#0a84ff;--accent-tint:rgba(10,132,255,.18);--accent-ring:rgba(10,132,255,.4);--danger:#ff453a;--danger-tint:rgba(255,69,58,.18);--ok:#30d158;--ok-tint:rgba(48,209,88,.18);--warn:#ffd60a;--warn-tint:rgba(255,214,10,.18);--sep:rgba(255,255,255,.12);--seg:#2c2c2e;--seg-active:#636366;--field:#2c2c2e;--row-hover:rgba(255,255,255,.04);--shadow:0 2px 12px rgba(0,0,0,.4);--shadow-seg:0 1px 3px rgba(0,0,0,.5)}
*,*::before,*::after{box-sizing:border-box}
[hidden]{display:none!important}
html{-webkit-text-size-adjust:100%}
body{margin:0;background:var(--bg);color:var(--text);font:17px/1.45 -apple-system,BlinkMacSystemFont,"SF Pro Text","Helvetica Neue",Helvetica,Arial,sans-serif;-webkit-font-smoothing:antialiased;overflow-x:hidden}
main{max-width:640px;margin:0 auto;padding:28px 16px 48px;display:flex;flex-direction:column;gap:20px}
header{display:flex;flex-direction:column;gap:4px;padding:8px 4px 0}
.title-row{display:flex;align-items:center;justify-content:space-between;gap:12px}
.theme-ctl{display:flex;align-items:center;gap:8px}
.theme-btn{background:var(--seg);color:var(--text)}
.theme-btn .ic-moon{display:none}
:root[data-theme="dark"] .theme-btn .ic-sun{display:none}
:root[data-theme="dark"] .theme-btn .ic-moon{display:block}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]) .theme-btn .ic-sun{display:none}:root:not([data-theme="light"]) .theme-btn .ic-moon{display:block}}
h1{margin:0;font-size:34px;line-height:1.1;font-weight:700;letter-spacing:-.02em}
.sub{margin:0;color:var(--text2);font-size:15px;overflow-wrap:anywhere}
.sub code{font:inherit;font-variant-numeric:tabular-nums}
h2{margin:0;font-size:22px;line-height:1.2;font-weight:600;letter-spacing:-.01em}
p{margin:0}
form{margin:0}
.muted{color:var(--text2)}
.small{font-size:15px}
.mono{font-variant-numeric:tabular-nums}
.seg{display:grid;grid-auto-flow:column;grid-auto-columns:1fr;gap:2px;padding:2px;background:var(--seg);border-radius:10px}
.seg a{display:flex;align-items:center;justify-content:center;height:34px;border-radius:8px;text-decoration:none;color:var(--text);font-size:15px;font-weight:500;transition:background .2s,box-shadow .2s}
.seg a[aria-current="page"]{background:var(--seg-active);box-shadow:var(--shadow-seg);font-weight:600}
.seg a:focus-visible{outline:none;box-shadow:0 0 0 3px var(--accent-ring)}
.card{background:var(--card);border-radius:18px;box-shadow:var(--shadow);padding:20px;display:flex;flex-direction:column;gap:16px}
.card.flush{padding:0;overflow:hidden}
.card.flush .card-head,.card.flush .lead{padding-left:20px;padding-right:20px}
.card.flush .card-head{padding-top:20px}
.card.flush .lead{padding-bottom:6px}
.card-head{display:flex;align-items:baseline;justify-content:space-between;gap:12px;flex-wrap:wrap}
section{display:flex;flex-direction:column;gap:16px}
.btn{appearance:none;border:0;cursor:pointer;font:inherit;font-size:17px;font-weight:500;line-height:1;padding:11px 20px;border-radius:980px;background:var(--accent);color:#fff;text-decoration:none;display:inline-flex;align-items:center;justify-content:center;transition:transform .12s,opacity .2s;white-space:nowrap}
.btn:active{transform:scale(.97)}
.btn:disabled,.btn[aria-disabled="true"]{opacity:.45;cursor:default;transform:none;pointer-events:none}
.btn.secondary{background:var(--accent-tint);color:var(--accent)}
.btn.destructive{background:var(--danger-tint);color:var(--danger)}
.btn.small{font-size:15px;padding:8px 16px}
.btn:focus-visible{outline:none;box-shadow:0 0 0 4px var(--accent-ring)}
.icon-btn{appearance:none;border:0;cursor:pointer;width:36px;height:36px;border-radius:50%;display:inline-flex;align-items:center;justify-content:center;background:var(--accent-tint);color:var(--accent);transition:transform .12s;flex:none;padding:0}
.icon-btn:active{transform:scale(.92)}
.icon-btn.destructive{background:var(--danger-tint);color:var(--danger)}
.icon-btn.playing{background:var(--accent);color:#fff}
.icon-btn:focus-visible{outline:none;box-shadow:0 0 0 4px var(--accent-ring)}
.ic{display:block;width:16px;height:16px;background:currentColor;-webkit-mask:var(--m) center/contain no-repeat;mask:var(--m) center/contain no-repeat}
.theme-btn .ic{width:18px;height:18px}
.ic-play{--m:url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><path d='M4 2.5v11l9-5.5z'/></svg>")}
.ic-trash{--m:url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16' fill='none' stroke='black' stroke-width='1.8' stroke-linecap='round'><path d='M2.5 4h11M6 4V2.5h4V4M4 4l.7 9.5h6.6L12 4M6.5 7v4M9.5 7v4'/></svg>")}
.ic-sun{--m:url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16' fill='none' stroke='black' stroke-width='1.6' stroke-linecap='round'><circle cx='8' cy='8' r='3.2'/><path d='M8 1.5v1.6M8 12.9v1.6M1.5 8h1.6M12.9 8h1.6M3.4 3.4l1.1 1.1M11.5 11.5l1.1 1.1M3.4 12.6l1.1-1.1M11.5 4.5l1.1-1.1'/></svg>")}
.ic-moon{--m:url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><path d='M9.6 1.8a6.3 6.3 0 1 0 4.6 8.7A5.2 5.2 0 0 1 9.6 1.8z'/></svg>")}
.ic-check{--m:url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 18 18' fill='none' stroke='black' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><path d='M3.5 9.5l3.5 3.5 7.5-8'/></svg>")}
.ic-lock{--m:url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 18 18' fill='none' stroke='black' stroke-width='1.6'><rect x='4' y='8' width='10' height='7' rx='1.5'/><path d='M6.5 8V6a2.5 2.5 0 015 0v2'/></svg>")}
.actions{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
.actions.between{justify-content:space-between}
.field{display:flex;flex-direction:column;gap:6px}
.field label{font-size:13px;font-weight:600;letter-spacing:.04em;text-transform:uppercase;color:var(--text2)}
.input{width:100%;font:inherit;font-size:17px;padding:12px 14px;border-radius:12px;border:1px solid var(--sep);background:var(--field);color:var(--text);outline:none;transition:box-shadow .15s,border-color .15s}
.input:focus{border-color:var(--accent);box-shadow:0 0 0 4px var(--accent-ring)}
.input::placeholder{color:var(--text3)}
.hint{font-size:13px;color:var(--text2)}
.row-inline{display:flex;gap:10px;align-items:stretch}
.row-inline .input{flex:1;min-width:0}
.pill{display:inline-flex;align-items:center;gap:6px;padding:4px 10px;border-radius:980px;font-size:13px;font-weight:600;line-height:1.3;font-variant-numeric:tabular-nums;white-space:nowrap}
.pill.ok{background:var(--ok-tint);color:var(--ok)}
.pill.warn{background:var(--warn-tint);color:var(--warn)}
.pill.danger{background:var(--danger-tint);color:var(--danger)}
.pill.accent{background:var(--accent-tint);color:var(--accent)}
.pill.neutral{background:var(--seg);color:var(--text2)}
.dot{width:8px;height:8px;border-radius:50%;background:currentColor}
.kv{display:grid;grid-template-columns:auto 1fr;gap:10px 20px;align-items:center;font-size:15px;margin:0}
.kv dt{margin:0;color:var(--text2)}
.kv dd{margin:0;text-align:right;overflow-wrap:anywhere;font-variant-numeric:tabular-nums}
.list{list-style:none;margin:0;padding:0}
.list li{display:flex;align-items:center;gap:14px;min-height:56px;padding:8px 20px;border-bottom:1px solid var(--sep)}
.list li:last-child{border-bottom:0}
.list li:hover{background:var(--row-hover)}
.list form{display:flex}
.name{flex:1;min-width:0;font-size:17px;overflow-wrap:anywhere;line-height:1.3}
.name .ext{color:var(--text3);font-size:13px;margin-left:4px}
.name .meta{display:block;font-size:13px;color:var(--text2)}
.empty{padding:28px 20px;text-align:center;color:var(--text2);font-size:15px}
.pager{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:14px 20px;border-top:1px solid var(--sep);font-size:15px;color:var(--text2)}
.progress{position:relative;height:6px;border-radius:980px;background:var(--seg);overflow:hidden}
.progress>i{position:absolute;top:0;bottom:0;left:0;width:0;background:var(--accent);border-radius:980px;transition:width .3s}
.dropzone{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:6px;padding:26px 16px;border:1.5px dashed var(--sep);border-radius:14px;background:var(--bg);text-align:center;color:var(--text2);font-size:15px;cursor:pointer}
.dropzone strong{color:var(--text);font-weight:600}
.dropzone input{display:none}
.nets{list-style:none;margin:0;padding:0;border:1px solid var(--sep);border-radius:14px;overflow:hidden}
.nets li{display:flex;align-items:center;gap:12px;padding:12px 14px;border-bottom:1px solid var(--sep);cursor:pointer;font-size:17px}
.nets li:last-child{border-bottom:0}
.nets li[aria-selected="true"]{background:var(--accent-tint)}
.nets .ssid{flex:1;min-width:0;overflow-wrap:anywhere}
.nets .ic{width:18px;height:18px;color:var(--text2);flex:none}
.nets .ic-check{visibility:hidden}
.nets li[aria-selected="true"] .ic-check{visibility:visible;color:var(--accent)}
.nets .ic-lock.open{visibility:hidden}
.nets .status{padding:14px;text-align:center;color:var(--text2);font-size:15px;cursor:default}
.bars{display:inline-flex;align-items:flex-end;gap:2px;height:14px;flex:none}
.bars i{width:3px;background:var(--text3);border-radius:1px}
.bars i:nth-child(1){height:5px}.bars i:nth-child(2){height:8px}.bars i:nth-child(3){height:11px}.bars i:nth-child(4){height:14px}
.bars[data-level="1"] i:nth-child(n+2),.bars[data-level="2"] i:nth-child(n+3),.bars[data-level="3"] i:nth-child(n+4){opacity:.25}
.banner{display:flex;align-items:center;gap:12px;padding:14px 16px;border-radius:14px;background:var(--accent-tint);color:var(--text);font-size:15px}
.banner.ok{background:var(--ok-tint)}
.banner.fail{background:var(--danger-tint)}
.spinner{width:18px;height:18px;border-radius:50%;border:2px solid var(--accent-ring);border-top-color:var(--accent);animation:spin .8s linear infinite;flex:none}
@keyframes spin{to{transform:rotate(360deg)}}
footer{text-align:center;font-size:13px;color:var(--text3);padding-top:8px}
@media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
@media (max-width:420px){h1{font-size:30px}.card{padding:16px}.card.flush{padding:0}.card.flush .card-head,.card.flush .lead{padding-left:16px;padding-right:16px}.card.flush .card-head{padding-top:16px}.list li{padding:8px 16px}.pager{padding:12px 16px}}
)CSS";

// Appearance toggle: cookie "theme=light|dark" overrides the system setting, no cookie = auto.
static const char THEME_JS[] = R"JS(<script>(function(){var r=document.documentElement,t=document.getElementById('tb'),a=document.getElementById('ta');function rc(){var m=document.cookie.match(/(?:^|; )theme=(light|dark)/);return m?m[1]:null}function wc(v){document.cookie='theme='+(v||'')+';path=/;max-age='+(v?31536000:0)+';SameSite=Lax'}function ef(){var v=r.getAttribute('data-theme');if(v==='light'||v==='dark')return v;return matchMedia('(prefers-color-scheme: dark)').matches?'dark':'light'}function ap(v){if(v)r.setAttribute('data-theme',v);else r.removeAttribute('data-theme');a.hidden=!v;t.setAttribute('aria-label',ef()==='dark'?'Switch to light mode':'Switch to dark mode')}ap(rc());t.onclick=function(){var n=ef()==='dark'?'light':'dark';wc(n);ap(n)};a.onclick=function(){wc(null);ap(null)}})()</script>)JS";

struct NavItem {
  const char *key;
  const char *href;
  const char *label;
};

// Order of the segmented control. Add an entry here to get a new tab everywhere.
static NavItem NAV_ITEMS[] = {
    {"files", "/", "Files"},
    {"upload", "/home", "Upload"},
    {"wifi", "/wifi", "Wi&#8209;Fi"},
};

// Set by the management server; produces the line under the box name (album folder).
String (*pageSubtitle)() = nullptr;
// Set by the management server; hides tabs that make no sense yet (Files/Upload without an album token).
bool (*navItemVisible)(const NavItem &item) = nullptr;

String htmlEscape(const String &in) {
  String out;
  out.reserve(in.length() + 8);
  for (unsigned int i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c;
    }
  }
  return out;
}

// Returns "light", "dark" or "" (follow the system) from the theme cookie.
const char *themeFromCookie(AsyncWebServerRequest *request) {
  if (!request->hasHeader("Cookie")) return "";
  const String &cookie = request->header("Cookie");
  int pos = cookie.indexOf("theme=");
  while (pos >= 0) {
    if (pos == 0 || cookie[pos - 1] == ' ' || cookie[pos - 1] == ';') {
      if (cookie.startsWith("dark", pos + 6)) return "dark";
      if (cookie.startsWith("light", pos + 6)) return "light";
    }
    pos = cookie.indexOf("theme=", pos + 6);
  }
  return "";
}

// Hash of the stylesheet, used as ?v= on the /css link so browsers fetch a new
// stylesheet after every firmware update instead of serving a cached one.
uint32_t cssVersion() {
  static uint32_t hash = [] {
    uint32_t h = 2166136261u;  // FNV-1a
    for (const char *p = PAGE_CSS; *p; p++) {
      h ^= (uint8_t)*p;
      h *= 16777619u;
    }
    return h;
  }();
  return hash;
}

void pageHead(AsyncResponseStream *res, AsyncWebServerRequest *request, const char *activeNav) {
  const char *theme = themeFromCookie(request);
  String subtitle = pageSubtitle ? pageSubtitle() : String("");
  res->print("<!DOCTYPE html><html lang=\"en\"");
  if (theme[0]) res->printf(" data-theme=\"%s\"", theme);
  res->printf("><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>" BOX_NAME "</title><link rel=\"stylesheet\" href=\"/css?v=%08x\"></head><body><main>", cssVersion());

  res->print("<header><div class=\"title-row\"><h1>" BOX_NAME "</h1><div class=\"theme-ctl\">"
             "<button class=\"btn secondary small\" type=\"button\" id=\"ta\" hidden>Auto</button>"
             "<button class=\"icon-btn theme-btn\" type=\"button\" id=\"tb\" aria-label=\"Switch appearance\"><i class=\"ic ic-sun\"></i><i class=\"ic ic-moon\"></i></button>"
             "</div></div>");
  res->printf("<p class=\"sub\">%s</p></header>", subtitle.c_str());

  res->print("<nav class=\"seg\" aria-label=\"Sections\">");
  for (const NavItem &item : NAV_ITEMS) {
    if (navItemVisible && !navItemVisible(item)) continue;
    bool active = strcmp(item.key, activeNav) == 0;
    res->printf("<a href=\"%s\"%s>%s</a>", item.href, active ? " aria-current=\"page\"" : "", item.label);
  }
  res->print("</nav><section>");
}

void pageFoot(AsyncResponseStream *res) {
  res->print("</section><footer>" BOX_NAME " &middot; management mode</footer>");
  res->print(THEME_JS);
  res->print("</main></body></html>");
}

void cssHandler(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(200, "text/css", (const uint8_t *)PAGE_CSS, sizeof(PAGE_CSS) - 1);
  response->addHeader("Cache-Control", "public, max-age=31536000, immutable");  // URL is versioned, see cssVersion()
  request->send(response);
}

#endif
