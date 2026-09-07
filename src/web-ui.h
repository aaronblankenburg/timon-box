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
.dropzone{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:6px;padding:26px 16px;border:1.5px dashed var(--sep);border-radius:14px;background:var(--bg);text-align:center;color:var(--text2);font-size:15px;cursor:pointer;transition:border-color .15s,background .15s}
.dropzone strong{color:var(--text);font-weight:600}
.dropzone.over{border-color:var(--accent);border-style:solid;background:var(--accent-tint);color:var(--accent)}
.dropzone.over strong{color:var(--accent)}
.ulist{list-style:none;margin:0;padding:0;border:1px solid var(--sep);border-radius:14px;overflow-x:hidden;overflow-y:auto;max-height:260px}
.ulist li{display:flex;align-items:center;gap:10px;padding:8px 12px;border-bottom:1px solid var(--sep);font-size:15px}
.ulist li:last-child{border-bottom:0}
.ulist .fn{flex:1;min-width:0;overflow-wrap:anywhere}
.ulist .fp{color:var(--text2);font-size:13px;font-variant-numeric:tabular-nums;white-space:nowrap}
.ulist .fz{color:var(--text3);font-size:13px;font-variant-numeric:tabular-nums;white-space:nowrap;min-width:4.5em;text-align:right}
.st{width:18px;height:18px;flex:none;border-radius:50%;border:1.5px solid var(--sep);display:flex;align-items:center;justify-content:center}
.st.up{border:2px solid var(--accent-ring);border-top-color:var(--accent);animation:spin .8s linear infinite}
.st.sv{border:2px solid var(--ok-tint);border-top-color:var(--ok);animation:spin .8s linear infinite}
.st.ok{border:0;background:var(--ok-tint);color:var(--ok)}
.st.err{border:0;background:var(--danger-tint);color:var(--danger)}
.st.ok::after,.st.err::after{content:"";width:11px;height:11px;background:currentColor;-webkit-mask:var(--m) center/contain no-repeat;mask:var(--m) center/contain no-repeat}
.st.ok::after{--m:url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 18 18' fill='none' stroke='black' stroke-width='2.4' stroke-linecap='round' stroke-linejoin='round'><path d='M3.5 9.5l3.5 3.5 7.5-8'/></svg>")}
.st.err::after{--m:url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 18 18' fill='none' stroke='black' stroke-width='2.4' stroke-linecap='round'><path d='M5 5l8 8M13 5l-8 8'/></svg>")}
.hint.danger{color:var(--danger)}
.actions.end{justify-content:flex-end}
dialog.modal{border:0;padding:0;margin:auto;background:transparent;color:var(--text);width:min(560px,calc(100vw - 32px));max-height:calc(100vh - 32px);overflow:visible}
dialog.modal::backdrop{background:rgba(0,0,0,.45);-webkit-backdrop-filter:blur(8px);backdrop-filter:blur(8px)}
dialog.modal .card{box-shadow:0 24px 64px rgba(0,0,0,.35);max-height:calc(100vh - 32px);overflow:auto}
dialog.modal .ulist{max-height:36vh}
.notice{display:flex;gap:10px;align-items:flex-start;padding:12px 14px;border-radius:12px;background:var(--warn-tint);color:var(--text);font-size:14px;line-height:1.4}
.notice .ic{width:18px;height:18px;flex:none;color:var(--warn);margin-top:1px}
.ic-warn{--m:url("data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 18 18' fill='none' stroke='black' stroke-width='1.7' stroke-linecap='round' stroke-linejoin='round'><path d='M9 2.5l7 12.5H2z'/><path d='M9 7v4M9 13.2v.1'/></svg>")}
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
html,.ulist,dialog.modal .card{scrollbar-width:thin;scrollbar-color:var(--text3) transparent}
::-webkit-scrollbar{width:8px;height:8px}
::-webkit-scrollbar-track{background:transparent}
::-webkit-scrollbar-thumb{background:var(--text3);border-radius:980px;border:2px solid transparent;background-clip:padding-box}
::-webkit-scrollbar-thumb:hover{background:var(--text2);background-clip:padding-box}
::-webkit-scrollbar-corner{background:transparent}
@media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
@media (max-width:420px){h1{font-size:30px}.card{padding:16px}.card.flush{padding:0}.card.flush .card-head,.card.flush .lead{padding-left:16px;padding-right:16px}.card.flush .card-head{padding-top:16px}.list li{padding:8px 16px}.pager{padding:12px 16px}}
)CSS";

// Page script for all pages, served as /app.js (versioned, cached, straight from flash so
// pages stay small and need little heap to render). Each part activates itself when its
// elements exist: theme toggle, album notice poll, upload page, Wi-Fi page.
static const char PAGE_JS[] = R"APPJS((function(){
var $=function(id){return document.getElementById(id)};
function esc(s){return String(s).replace(/[&<>"']/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]})}
function fmt(n){if(n>=1073741824)return (n/1073741824).toFixed(2)+' GB';if(n>=1048576)return (n/1048576).toFixed(1)+' MB';return Math.max(1,Math.round(n/1024))+' kB'}
function fmtT(s){if(s<60)return Math.ceil(s)+' s';var m=Math.floor(s/60);return m+' min '+Math.round(s-m*60)+' s'}

// Appearance: cookie "theme=light|dark" overrides the system setting, no cookie = auto.
(function(){var r=document.documentElement,t=$('tb'),a=$('ta');if(!t||!a)return;
function rc(){var m=document.cookie.match(/(?:^|; )theme=(light|dark)/);return m?m[1]:null}
function wc(v){document.cookie='theme='+(v||'')+';path=/;max-age='+(v?31536000:0)+';SameSite=Lax'}
function ef(){var v=r.getAttribute('data-theme');if(v==='light'||v==='dark')return v;return matchMedia('(prefers-color-scheme: dark)').matches?'dark':'light'}
function ap(v){if(v)r.setAttribute('data-theme',v);else r.removeAttribute('data-theme');a.hidden=!v;t.setAttribute('aria-label',ef()==='dark'?'Switch to light mode':'Switch to dark mode')}
ap(rc());t.onclick=function(){var n=ef()==='dark'?'light':'dark';wc(n);ap(n)};a.onclick=function(){wc(null);ap(null)}})();

// "Which album?" notice: poll until an album token is recognised, then show its files.
if($('an')){(function(){function c(){fetch('/album',{cache:'no-store'}).then(function(r){return r.json()}).then(function(d){if(d.album)location.href='/';else setTimeout(c,2000)}).catch(function(){setTimeout(c,3000)})}setTimeout(c,2000)})()}

// Upload page: selection, modal dialog, PAR uploads at a time (each connection is capped by the
// box's small TCP window), byte-accurate progress, per-file status, retry.
if($('f')){(function(){
var i=$('f'),z=$('dz'),b=$('bar'),t=$('pt'),r=$('pr'),ul=$('ul'),ph=$('ph'),u=$('ub'),rb=$('rb'),dlg=$('dlg'),mt=$('mt'),mh=$('mh'),mc=$('mc'),mx=$('mx'),ms=$('ms'),slot=$('slot'),mslot=$('mslot');
var SD_FREE=+($('uc').dataset.sdfree||0),PAR=+($('uc').dataset.par||2);
var items=[],total=0,doneBytes=0,startAt=0,samples=[],busy=false,cancelled=false,q=[],active=0,failed=0,inflight={};
function fp(k){return $('it'+k).querySelector('.fp')}
function setFiles(list){if(busy)return;var a=[],skipped=0;for(var k=0;k<list.length;k++){var f=list[k];if(f.type==='audio/mpeg'||/\.mp3$/i.test(f.name))a.push(f);else skipped++}
items=a.map(function(f){return{file:f,status:'pending',loaded:0}});total=0;items.forEach(function(x){total+=x.file.size});doneBytes=0;b.style.width='0';u.textContent='Upload';
ul.hidden=!items.length;ul.innerHTML=items.map(function(x,k){return '<li id="it'+k+'"><i class="st"></i><span class="fn">'+esc(x.file.name)+'</span><span class="fp"></span><span class="fz">'+fmt(x.file.size)+'</span></li>'}).join('');
if(!items.length){ph.textContent=skipped?'Only MP3 files can be uploaded':'No files selected';ph.className='hint';u.disabled=true;return}
var s=items.length+' file'+(items.length>1?'s':'')+' · '+fmt(total)+(skipped?' · '+skipped+' skipped (not MP3)':'');
if(total>SD_FREE){ph.textContent=s+' · not enough space on the SD card ('+fmt(SD_FREE)+' free)';ph.className='hint danger';u.disabled=true}
else{ph.textContent=s+' · '+fmt(SD_FREE-total)+' will stay free';ph.className='hint';u.disabled=false}}
function setSt(k,st,txt){items[k].status=st;var li=$('it'+k);li.firstChild.className='st '+({uploading:'up',saving:'sv',done:'ok',failed:'err'}[st]||'');fp(k).textContent=txt||''}
function progressAll(){var sent=doneBytes,done=0,act=0;items.forEach(function(x){if(x.status==='done')done++;else if(x.status==='uploading'||x.status==='saving'){act++;sent+=x.loaded}});
var pct=total?Math.min(100,Math.round(sent/total*100)):0;b.style.width=pct+'%';
t.textContent=(act?'Uploading · ':'')+done+' of '+items.length+' done'+(act>1?' · '+act+' at once':'')+' · '+fmt(sent)+' of '+fmt(total)+' · '+pct+' %';
var now=Date.now();samples.push([now,sent]);while(samples.length>2&&now-samples[0][0]>4000)samples.shift();
if(samples.length>1){var a=samples[0],e=samples[samples.length-1],rate=e[0]>a[0]?(e[1]-a[1])/((e[0]-a[0])/1000):0;
if(now-startAt>2500&&rate>0)r.textContent=fmt(rate)+'/s · ~'+fmtT((total-sent)/rate)+' left'}}
function uploadOne(k,cb){var x=items[k],xhr=new XMLHttpRequest();inflight[k]=xhr;x.loaded=0;xhr.open('POST','/upload?name='+encodeURIComponent(x.file.name));xhr.setRequestHeader('Content-Type','application/octet-stream');setSt(k,'uploading','0 %');
xhr.upload.onprogress=function(e){if(!e.lengthComputable)return;x.loaded=e.loaded;progressAll();fp(k).textContent=Math.round(e.loaded/e.total*100)+' %'};
xhr.upload.onload=function(){x.loaded=x.file.size;setSt(k,'saving','saving')};
function end(ok,st,txt){delete inflight[k];if(ok)doneBytes+=x.file.size;x.loaded=0;setSt(k,st,txt);progressAll();cb(ok)}
xhr.onload=function(){if(xhr.status>=200&&xhr.status<300)end(true,'done');else end(false,'failed','HTTP '+xhr.status)};
xhr.onerror=xhr.ontimeout=function(){end(false,'failed','failed')};
xhr.onabort=function(){end(false,'failed','cancelled')};
xhr.send(x.file)}
function openModal(){mslot.appendChild(ul);ul.hidden=false;if(dlg.showModal){if(!dlg.open)dlg.showModal()}else dlg.setAttribute('open','')}
function closeModal(){if(dlg.close){if(dlg.open)dlg.close()}else dlg.removeAttribute('open');slot.appendChild(ul)}
dlg.addEventListener('cancel',function(e){if(busy)e.preventDefault()});dlg.addEventListener('close',function(){slot.appendChild(ul)});
window.addEventListener('beforeunload',function(e){if(busy){e.preventDefault();e.returnValue=''}});
function run(){q=[];items.forEach(function(x,k){if(x.status!=='done')q.push(k)});if(!q.length||busy)return;
busy=true;cancelled=false;failed=0;active=0;u.disabled=true;i.disabled=true;rb.hidden=true;mx.hidden=true;ms.hidden=true;mc.hidden=false;mh.hidden=false;
mt.textContent='Uploading';t.textContent='Starting…';r.textContent='';startAt=Date.now();samples=[];
doneBytes=0;items.forEach(function(x){if(x.status==='done')doneBytes+=x.file.size});progressAll();openModal();pump()}
function pump(){while(!cancelled&&active<PAR&&q.length){active++;uploadOne(q.shift(),function(ok){active--;if(!ok)failed++;if(active===0&&(cancelled||!q.length))finish(failed);else pump()})}}
function finish(failed){busy=false;i.disabled=false;r.textContent='';mc.hidden=true;mh.hidden=true;
var pending=items.filter(function(x){return x.status!=='done'}).length;u.disabled=!pending;u.textContent=pending&&pending<items.length?'Upload remaining':'Upload';
if(cancelled){mt.textContent='Upload cancelled';t.textContent=(items.length-pending)+' of '+items.length+' uploaded · '+fmt(doneBytes);rb.textContent='Continue upload';rb.hidden=false;mx.hidden=false}
else if(failed){mt.textContent='Upload incomplete';t.textContent=failed+' of '+items.length+' failed · '+fmt(doneBytes)+' uploaded. Check the connection, then retry.';rb.textContent='Retry failed';rb.hidden=false;mx.hidden=false}
else{mt.textContent='Done';t.textContent=items.length+' file'+(items.length>1?'s':'')+' · '+fmt(total)+' uploaded';ms.hidden=false;mx.hidden=false}}
i.onchange=function(){setFiles(i.files)};
['dragenter','dragover'].forEach(function(ev){z.addEventListener(ev,function(e){e.preventDefault();if(!busy)z.classList.add('over')})});
['dragleave','drop'].forEach(function(ev){z.addEventListener(ev,function(e){e.preventDefault();z.classList.remove('over')})});
z.addEventListener('drop',function(e){setFiles(e.dataTransfer.files)});
document.addEventListener('dragover',function(e){e.preventDefault()});document.addEventListener('drop',function(e){e.preventDefault()});
u.onclick=run;rb.onclick=run;mc.onclick=function(){if(!busy)return;cancelled=true;Object.keys(inflight).forEach(function(k){inflight[k].abort()})};mx.onclick=closeModal;ms.onclick=function(){location.href='/'};
})()}

// Wi-Fi page: network list from /wifi-scan, connection result from /wifi-status, hand-over to <box>.local.
if($('nl')){(function(){
var q=location.search,nl=$('nl'),si=$('ssid'),bn=$('bn'),jh=$('jh'),mp=$('mp');
var cfg=$('wf'),HOST=cfg.dataset.host,station=cfg.dataset.station==='1',TARGET='http://'+HOST+'/wifi',onHost=location.hostname.toLowerCase()===HOST,done=false;
function lv(r){return r>-55?4:r>-65?3:r>-75?2:1}
function banner(c,h){bn.innerHTML=h?'<div class="banner '+c+'">'+h+'</div>':''}
function render(d){if(d.state==='done'&&d.networks.length){nl.innerHTML=d.networks.map(function(n){return '<li role="option" data-ssid="'+esc(n.ssid)+'" aria-selected="'+(n.ssid===si.value)+'"><i class="ic ic-check"></i><span class="ssid">'+esc(n.ssid)+'</span><i class="ic ic-lock'+(n.secure?'':' open')+'"></i><span class="bars" data-level="'+lv(n.rssi)+'"><i></i><i></i><i></i><i></i></span></li>'}).join('')}
else if(d.state==='done')nl.innerHTML='<li class="status">No networks found</li>';
else if(d.state==='failed')nl.innerHTML='<li class="status">No networks found yet. Tap Rescan to look again, or type the name below.</li>';
else if(d.state==='busy')nl.innerHTML='<li class="status">Waiting for the connection test to finish…</li>';
else nl.innerHTML='<li class="status">Scanning…</li>'}
var st;function scan(re){clearTimeout(st);fetch('/wifi-scan'+(re?'?rescan=1':'')).then(function(r){return r.json()}).then(function(d){render(d);if(d.state!=='done'&&d.state!=='failed')st=setTimeout(function(){scan(false)},1500)}).catch(function(){st=setTimeout(function(){scan(false)},2000)})}
nl.onclick=function(e){var li=e.target.closest('li[data-ssid]');if(!li)return;nl.querySelectorAll('li').forEach(function(x){x.setAttribute('aria-selected','false')});li.setAttribute('aria-selected','true');si.value=li.dataset.ssid;$('pass').focus()};
$('rs').onclick=function(){scan(true)};scan(false);
jh.textContent=station?'Switching networks disconnects this page. Reach the box again via '+HOST+' or the hotspot.':'The hotspot stays up during the test. Once connected it shuts down and this page moves to '+HOST+'.';
function tryHost(fail){if(onHost)return fail();fetch('http://'+HOST+'/wifi-status',{cache:'no-store'}).then(function(r){if(r.ok)location.href=TARGET;else fail()}).catch(fail)}
function hop(){tryHost(function(){setTimeout(hop,2000)})}
function poll(){fetch('/wifi-status',{cache:'no-store'}).then(function(r){return r.json()}).then(function(d){
if(d.result==='ok'){done=true;banner('ok','<span>&#10003;</span><span><b>Connected</b> to '+esc(d.ssid)+' as <span class="mono">'+d.ip+'</span>. '+(d.hotspotClosing?'The hotspot shuts down now. Rejoin your home network; this page then moves to <a href="'+TARGET+'">'+HOST+'</a>. If that does not happen, open <a href="http://'+d.ip+'/wifi">http://'+d.ip+'/wifi</a>.':'Reachable at <a href="'+TARGET+'">'+HOST+'</a>.')+'</span>');
mp.className='pill ok';mp.innerHTML='<span class="dot"></span>Home network';$('k-mode').innerHTML='Home network'+(d.hotspotClosing?' <span class="muted">(hotspot shutting down)</span>':'');$('k-net').textContent=d.ssid+' · '+d.rssi+' dBm';$('k-ip').textContent=d.ip;$('k-saved').textContent=d.saved;
if(!onHost)setTimeout(hop,3000)}
else if(d.result==='failed'){banner('fail','<span>&#10007;</span><span><b>Could not connect.</b> '+esc(d.hint)+' The network is saved anyway; correct the password and try again.</span>');$('k-saved').textContent=d.saved}
else setTimeout(poll,1000)}).catch(function(){if(done)return;tryHost(function(){setTimeout(poll,1500)})})}
if(/connecting=1/.test(q)){banner('','<span class="spinner"></span><span>Connecting… this page keeps checking.</span>');poll()}
if(/forgot=1/.test(q))banner('','<span>Saved network removed. The box runs as its own hotspot again.</span>');
if(/err=1/.test(q))banner('fail','<span>Please enter a network name (1–32 characters) and a password of 8–63 characters, or none for an open network.</span>');
if(/err=2/.test(q))banner('fail','<span>The box is busy, please try again.</span>');
})()}

// Copy page: pick the other box, list its album, copy the tracks missing here.
// The transfer runs on the box, so this only queues jobs and polls the status;
// a reload or a closed browser does not stop it. Progress is shown in the same
// kind of dialog the upload uses, with the file list moved into it.
if($('cf')){(function(){
var pl=$('pl'),ps=$('ps'),peer=$('peer'),cn=$('cn'),ct=$('ct'),cr=$('cr'),cs=$('cs'),cu=$('cu'),cw=$('cw'),cg=$('cg'),
cslot=$('cslot'),dlg=$('cd'),cdt=$('cdt'),cdr=$('cdr'),cdb=$('cdb'),cds=$('cds'),cmslot=$('cmslot'),cdh=$('cdh'),cdc=$('cdc'),cdx=$('cdx'),cdg=$('cdg'),cdf=$('cdf');
var q=location.search,ST={pending:'',copying:'up',done:'ok',failed:'err',present:'ok'},samples=[],wasCopying=false,closed=false;
function note(c,h){cn.innerHTML='<div class="banner '+c+'">'+h+'</div>'}
if(/err=1/.test(q))note('fail','<span>Enter the name or address of the other box.</span>');
if(/err=2/.test(q))note('fail','<span>The box is busy with another transfer.</span>');
function open(){if(dlg.showModal){if(!dlg.open)dlg.showModal()}else dlg.setAttribute('open','')}
function close(){if(dlg.close){if(dlg.open)dlg.close()}else dlg.removeAttribute('open')}
dlg.addEventListener('close',function(){closed=true;cslot.appendChild(cu)});
function label(d,f,i){
if(i===d.current&&f.status==='copying'&&f.size)return Math.min(99,Math.round(d.bytesCur/f.size*100))+' %';
return f.status==='present'?'already here':f.status==='failed'?'failed':f.status==='done'?'copied':''}
function rate(d){var now=Date.now();samples.push([now,d.bytes]);while(samples.length>2&&now-samples[0][0]>8000)samples.shift();
if(samples.length<2)return '';var a=samples[0],e=samples[samples.length-1];if(e[0]-a[0]<3000)return '';
var r=(e[1]-a[1])/((e[0]-a[0])/1000);if(r<=0)return '';
return fmt(r)+'/s · ~'+fmtT((d.bytesTotal-d.bytes)/r)+' left'}
function render(d){
var files=d.files||[],copying=d.state==='copying',pct=d.bytesTotal?Math.min(100,Math.round(d.bytes/d.bytesTotal*100)):0,job=files.filter(function(f){return f.status!=='present'}).length,tr=function(n){return n+(n===1?' track':' tracks')};
ct.textContent=d.album?'Album: '+d.album:'Album';
cu.hidden=!files.length;
cu.innerHTML=files.map(function(f,i){return '<li><i class="st '+(ST[f.status]||'')+'"></i><span class="fn">'+esc(f.name)+'</span><span class="fp">'+label(d,f,i)+'</span><span class="fz">'+fmt(f.size)+'</span></li>'}).join('');
if(copying&&!wasCopying){closed=false;samples=[]}
if(copying&&!closed){cmslot.appendChild(cu);open()}
cw.hidden=!(copying&&closed);
cg.hidden=copying||!d.missing||!d.station;
if(d.missing)cg.textContent=(d.failed?'Retry ':'Copy ')+d.missing+' missing file'+(d.missing>1?'s':'');
// card summary
if(d.state==='listing')cs.textContent='Loading the album from '+(d.peer||'the other box')+'…';
else if(copying)cs.textContent='Copying '+d.done+' of '+job+' · '+pct+' %';
else if(d.state==='failed')cs.textContent=d.error||'The transfer failed.';
else if(d.state==='cancelled')cs.textContent='Cancelled · '+d.done+' of '+job+' copied.';
else if(d.done&&!d.missing)cs.textContent=tr(d.done)+' copied · the album is complete on this box.';
else if(files.length)cs.textContent=d.missing?d.missing+' of '+tr(d.total)+' missing here':'All '+tr(d.total)+' are already here.';
else cs.textContent='No album loaded yet.';
cs.className=d.state==='failed'?'hint danger':'hint';
cr.textContent=copying?pct+' %':'';
// dialog
cdb.style.width=pct+'%';
cdr.textContent=copying?rate(d):'';
if(copying){cdt.textContent='Copying';cds.textContent=(d.current>=0?'File '+(d.current+1)+' of '+d.total+' · ':'')+fmt(d.bytes)+' of '+fmt(d.bytesTotal)+' · '+pct+' %'}
else if(d.state==='done'){cdt.textContent='Done';cds.textContent=tr(d.done)+' copied · '+fmt(d.bytesTotal)}
else if(d.state==='failed'&&wasCopying){cdt.textContent='Copy incomplete';cds.textContent=d.error||(d.failed+' files failed.')}
else if(d.state==='cancelled'){cdt.textContent='Copy cancelled';cds.textContent=d.done+' of '+job+' copied · '+fmt(d.bytes)}
cdc.hidden=!copying;cdh.hidden=!copying;
cdx.hidden=copying;
cdg.hidden=copying||!d.missing||!wasCopying;
if(d.missing)cdg.textContent='Retry '+d.missing+' file'+(d.missing>1?'s':'');
cdf.hidden=copying||!d.done;
wasCopying=copying}
function poll(){fetch('/copy-status',{cache:'no-store'}).then(function(r){return r.json()}).then(function(d){render(d);if(d.state==='listing'||d.state==='copying')setTimeout(poll,1000)}).catch(function(){setTimeout(poll,2000)})}
poll();
cw.onclick=function(){closed=false;cmslot.appendChild(cu);open()};
cdx.onclick=close;cdf.onclick=function(){location.href='/'};
function scan(re){fetch('/copy-scan'+(re?'?rescan=1':'')).then(function(r){return r.json()}).then(function(d){
if(d.state==='busy'){pl.innerHTML='<li class="status">Busy with a transfer</li>';return}
if(d.state==='scanning'){pl.innerHTML='<li class="status">Searching…</li>';setTimeout(function(){scan(false)},1200);return}
if(!d.boxes.length){pl.innerHTML='<li class="status">No other box found. Type its address below; it needs its management token in place.</li>';return}
pl.innerHTML=d.boxes.map(function(b){return '<li data-host="'+esc(b.host)+'" aria-selected="'+(b.host===peer.value)+'"><i class="ic ic-check"></i><span class="ssid">'+esc(b.name)+'</span><span class="muted small">'+esc(b.host)+'</span></li>'}).join('')})
.catch(function(){pl.innerHTML='<li class="status">Search failed</li>'})}
ps.onclick=function(){pl.innerHTML='<li class="status">Searching…</li>';scan(true)};
pl.onclick=function(e){var li=e.target.closest('li[data-host]');if(!li)return;peer.value=li.dataset.host;pl.querySelectorAll('li').forEach(function(x){x.setAttribute('aria-selected','false')});li.setAttribute('aria-selected','true')};
})()}
})();
)APPJS";

struct NavItem {
  const char *key;
  const char *href;
  const char *label;
};

// Order of the segmented control. Add an entry here to get a new tab everywhere.
static NavItem NAV_ITEMS[] = {
    {"files", "/", "Files"},
    {"upload", "/home", "Upload"},
    {"copy", "/copy", "Copy"},
    {"wifi", "/wifi", "Wi&#8209;Fi"},
};

// Set by the management server; produces the line under the box name (album folder).
String (*pageSubtitle)() = nullptr;
// Set by the management server; hides tabs that make no sense yet (Files/Upload without an album token).
bool (*navItemVisible)(const NavItem &item) = nullptr;
// Set by the management server; prints a notice above the page content (e.g. "place an album token").
void (*pageNotice)(AsyncResponseStream *res) = nullptr;

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

// Content hashes used as ?v= on the /css and /app.js links so browsers fetch fresh
// copies after every firmware update while caching them for a year otherwise.
static uint32_t fnv1a(const char *p) {
  uint32_t h = 2166136261u;
  for (; *p; p++) {
    h ^= (uint8_t)*p;
    h *= 16777619u;
  }
  return h;
}

uint32_t cssVersion() {
  static uint32_t hash = fnv1a(PAGE_CSS);
  return hash;
}

uint32_t jsVersion() {
  static uint32_t hash = fnv1a(PAGE_JS);
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
  if (pageNotice) pageNotice(res);
}

void pageFoot(AsyncResponseStream *res) {
  res->print("</section><footer>" BOX_NAME " &middot; management mode</footer>");
  res->printf("<script src=\"/app.js?v=%08x\"></script>", jsVersion());
  res->print("</main></body></html>");
}

void cssHandler(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(200, "text/css", (const uint8_t *)PAGE_CSS, sizeof(PAGE_CSS) - 1);
  response->addHeader("Cache-Control", "public, max-age=31536000, immutable");  // URL is versioned, see cssVersion()
  request->send(response);
}

void jsHandler(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(200, "application/javascript", (const uint8_t *)PAGE_JS, sizeof(PAGE_JS) - 1);
  response->addHeader("Cache-Control", "public, max-age=31536000, immutable");  // URL is versioned, see jsVersion()
  request->send(response);
}

#endif
