#include "web_ui.h"

#include <WiFi.h>

#include <vector>

#include "diagnostics/log.h"
#include "diagnostics/log_buffer.h"
#include "diagnostics/system_info.h"
#include "project_config.h"

namespace zivyobraz::web {

// ---------------------------------------------------------------------------
// Single-page application HTML stored in flash (PROGMEM).
// The page has four tabs: Živý obraz, Konfigurace, Správa, Debug.
// ---------------------------------------------------------------------------
static const char kHtmlPage[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="cs">
<head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Živý obraz</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#0d1117;color:#c9d1d9;min-height:100vh}
header{background:#161b22;padding:12px 20px;border-bottom:2px solid #30363d;display:flex;align-items:center;gap:10px}
header h1{font-size:1.1em;color:#e94560}
.sb{background:#0d1117;border-bottom:1px solid #21262d;padding:4px 20px;font-size:12px;color:#8b949e;font-family:monospace;min-height:22px}
nav{display:flex;background:#161b22;border-bottom:1px solid #30363d}
.tb{padding:10px 16px;background:none;border:none;border-bottom:2px solid transparent;color:#8b949e;cursor:pointer;font-size:13px;transition:color .2s,border-color .2s}
.tb:hover{color:#c9d1d9}
.tb.act{color:#e94560;border-bottom-color:#e94560}
.tp{display:none;padding:20px;max-width:960px}
.tp.act{display:block}
h2{font-size:.95em;color:#e94560;margin-bottom:14px;padding-bottom:6px;border-bottom:1px solid #21262d}
img#prev{display:block;max-width:100%;image-rendering:pixelated;border:1px solid #30363d;margin:0 auto;background:#111}
label{display:block;margin-top:10px;font-size:13px;color:#8b949e}
input[type=text],input[type=password]{width:100%;max-width:380px;padding:7px 10px;background:#161b22;border:1px solid #30363d;border-radius:4px;color:#c9d1d9;font-size:13px;margin-top:3px}
input[type=text]:focus,input[type=password]:focus{outline:none;border-color:#e94560}
.cb-row{display:flex;align-items:center;gap:8px;margin-top:10px;font-size:13px;color:#c9d1d9}
.btn{margin-top:14px;padding:7px 16px;background:#e94560;color:#fff;border:none;border-radius:4px;cursor:pointer;font-size:13px}
.btn:hover{background:#c73752}
.btn.sec{background:#21262d;color:#c9d1d9;border:1px solid #30363d}
.btn.sec:hover{background:#30363d}
.cards{display:flex;flex-wrap:wrap;gap:12px;margin-bottom:16px}
.card{background:#161b22;border:1px solid #30363d;border-radius:6px;padding:10px 14px;min-width:160px}
.card dt{color:#8b949e;font-size:11px;margin-bottom:2px}
.card dd{color:#c9d1d9;font-size:13px;font-family:monospace;word-break:break-all}
pre#lg{background:#161b22;border:1px solid #30363d;border-radius:4px;padding:12px;height:400px;overflow-y:auto;font-size:12px;font-family:monospace;white-space:pre-wrap;word-break:break-all}
.lI{color:#79c0ff}.lW{color:#e3b341}.lE{color:#f85149}
.msg{padding:7px 12px;border-radius:4px;margin-top:10px;font-size:13px}
.ok{background:#0d4a23;color:#56d364}
.er{background:#4c1d1d;color:#f85149}
.dbg-ctrl{display:flex;gap:8px;align-items:center;margin-bottom:8px}
</style>
</head>
<body>
<header><span style="font-size:1.4em">&#128250;</span><h1>&#381;iv&#253; obraz &#8211; LCD panel</h1></header>
<div class="sb" id="sb">Připojování&#8230;</div>
<nav>
<button class="tb act" onclick="showTab('live',this)">&#128247; Živý obraz</button>
<button class="tb" onclick="showTab('cfg',this)">&#9881;&#65039; Konfigurace</button>
<button class="tb" onclick="showTab('adm',this)">&#128295; Správa</button>
<button class="tb" onclick="showTab('dbg',this)">&#128027; Debug</button>
</nav>
<div id="live" class="tp act">
<p style="font-size:13px;color:#8b949e;margin-bottom:12px">Aktuální obraz zobrazený na LCD (automatické obnovení každých 30&nbsp;s)</p>
<img id="prev" src="/api/preview.bmp" alt="Náhled obrazu" width="240" height="240">
<div style="text-align:center"><button class="btn sec" style="margin-top:10px" onclick="rfImg()">&#128260; Obnovit nyní</button></div>
</div>
<div id="cfg" class="tp">
<h2>Konfigurace WiFi a serveru</h2>
<form onsubmit="return false">
<label>Dostupné WiFi sítě
<select id="c-scan" style="width:100%;max-width:380px;padding:7px 10px;background:#161b22;border:1px solid #30363d;border-radius:4px;color:#c9d1d9;font-size:13px;margin-top:3px" onchange="pickScan()">
<option value="">-- Ručně zadejte SSID nebo vyberte síť --</option>
</select>
</label>
<button class="btn sec" onclick="scanWifi()" type="button" style="margin-top:8px">&#128246; Proskenovat sítě</button>
<label>WiFi SSID<input type="text" id="c-ssid" maxlength="32" placeholder="NázevSítě"></label>
<label>WiFi heslo (prázdné = zachovat stávající)<input type="password" id="c-pass" maxlength="64" placeholder="&#8226;&#8226;&#8226;&#8226;&#8226;&#8226;&#8226;&#8226;"></label>
<label>Server host<input type="text" id="c-host" maxlength="128" placeholder="cdn.zivyobraz.eu"></label>
<div class="cb-row"><input type="checkbox" id="c-https"><label for="c-https" style="margin:0;cursor:pointer">Použít HTTPS</label></div>
<button class="btn" onclick="saveCfg()">&#128190; Uložit konfiguraci</button>
</form>
<div id="cm" class="msg" style="display:none"></div>
</div>
<div id="adm" class="tp">
<h2>Informace o zařízení</h2>
<div class="cards" id="ai"></div>
<h2 style="margin-top:8px">Správa</h2>
<button class="btn" onclick="doRestart()">&#128260; Restartovat zařízení</button>
<div id="am" class="msg" style="display:none"></div>
</div>
<div id="dbg" class="tp">
<h2>Výstup ladění (real-time)</h2>
<div class="dbg-ctrl">
<button class="btn sec" onclick="clrLog()">&#128465; Vymazat</button>
<label style="display:inline;margin:0;font-size:13px"><input type="checkbox" id="asc" checked> Auto-scroll</label>
</div>
<pre id="lg"></pre>
</div>
<script>
var ll=[],lp=0,lt=false;
function showTab(id,btn){
  ['live','cfg','adm','dbg'].forEach(function(t){document.getElementById(t).className='tp';});
  document.querySelectorAll('.tb').forEach(function(b){b.className='tb';});
  document.getElementById(id).className='tp act';
  btn.className='tb act';
  if(id==='dbg'&&!lt){lt=true;pollLog();}
  if(id==='adm')loadAdm();
  if(id==='cfg')loadCfg();
}
function rfImg(){var i=document.getElementById('prev');i.src='/api/preview.bmp?_='+Date.now();}
function loadSb(){
  fetch('/api/status').then(function(r){return r.json();}).then(function(d){
    var mode=d.portalActive?'Konfigurační AP':'STA';
    document.getElementById('sb').textContent='Režim: '+mode+' | WiFi: '+d.wifi+' | IP: '+d.ip+' | RSSI: '+d.rssi+' dBm | API klíč: '+d.apiKey;
  }).catch(function(){document.getElementById('sb').textContent='Nelze načíst stav';});
}
function loadCfg(){
  fetch('/api/status').then(function(r){return r.json();}).then(function(d){
    document.getElementById('c-ssid').value=d.ssid||'';
    document.getElementById('c-pass').value='';
    document.getElementById('c-host').value=d.host||'';
    document.getElementById('c-https').checked=!!d.httpsEnabled;
  }).catch(function(){});
}

function pickScan(){
  var ssid=document.getElementById('c-scan').value;
  if(ssid){document.getElementById('c-ssid').value=ssid;}
}
function scanWifi(){
  fetch('/api/wifi/scan').then(function(r){return r.json();}).then(function(d){
    if(!d.ok){showM('cm','Scan selhal: '+(d.error||''),false);return;}
    var sel=document.getElementById('c-scan');
    sel.innerHTML='<option value="">-- Ručně zadejte SSID nebo vyberte síť --</option>';
    (d.networks||[]).forEach(function(n){
      var o=document.createElement('option');
      o.value=n.ssid;
      o.textContent=n.ssid+' ('+n.rssi+' dBm'+(n.secure?', zabezpečeno':'')+')';
      sel.appendChild(o);
    });
    showM('cm','WiFi scan dokončen, nalezeno sítí: '+((d.networks||[]).length),true);
  }).catch(function(){showM('cm','Scan selhal (chyba komunikace)',false);});
}

function saveCfg(){
  var body='ssid='+enc(document.getElementById('c-ssid').value)
          +'&pass='+enc(document.getElementById('c-pass').value)
          +'&host='+enc(document.getElementById('c-host').value)
          +'&https='+(document.getElementById('c-https').checked?'1':'0');
  fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body})
  .then(function(r){return r.json();}).then(function(d){
    var msg=d.ok?'Konfigurace uložena. ':'Chyba: '+(d.error||'');
    if(d.ok&&d.connected){msg+='WiFi připojeno, AP režim vypnut.';}
    if(d.ok&&!d.connected){msg+='Připojení selhalo, AP režim zůstává aktivní.';}
    showM('cm',msg,d.ok&&!!d.connected);
  }).catch(function(){showM('cm','Chyba komunikace',false);});
}
function showM(id,txt,ok){
  var el=document.getElementById(id);
  el.style.display='block';
  el.className='msg '+(ok?'ok':'er');
  el.textContent=txt;
}
function loadAdm(){
  fetch('/api/status').then(function(r){return r.json();}).then(function(d){
    var items=[['FW verze',d.fwVersion],['API verze',d.apiVersion],['Build datum',d.buildDate],
               ['Čip',d.chip],['Flash',d.flashSize],['SDK',d.sdk],['Uptime',d.uptime],['Volná RAM',d.freeHeap]];
    document.getElementById('ai').innerHTML=items.map(function(i){
      return '<div class="card"><dt>'+i[0]+'</dt><dd>'+esc(String(i[1]))+'</dd></div>';
    }).join('');
  }).catch(function(){});
}
function doRestart(){
  if(!confirm('Opravdu restartovat zařízení?'))return;
  fetch('/api/restart',{method:'POST'}).then(function(){
    showM('am','Zařízení se restartuje… stránka se obnoví za 8 s.',true);
    setTimeout(function(){location.reload();},8000);
  }).catch(function(){showM('am','Chyba',false);});
}
function pollLog(){
  if(!document.getElementById('dbg').classList.contains('act')){lt=false;return;}
  fetch('/api/log?since='+lp).then(function(r){return r.json();}).then(function(d){
    if(d.lines&&d.lines.length){d.lines.forEach(function(l){ll.push(l);});lp=d.next;renderLog();}
    setTimeout(pollLog,1000);
  }).catch(function(){setTimeout(pollLog,2000);});
}
function renderLog(){
  var p=document.getElementById('lg');
  var h=ll.slice(-300).map(function(l){
    var c=l.charAt(1)==='E'?'lE':l.charAt(1)==='W'?'lW':'lI';
    return '<span class="'+c+'">'+esc(l)+'</span>';
  }).join('\n');
  p.innerHTML=h;
  if(document.getElementById('asc').checked)p.scrollTop=p.scrollHeight;
}
function clrLog(){ll=[];document.getElementById('lg').innerHTML='';}
function esc(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}
function enc(s){return encodeURIComponent(s);}
setInterval(function(){if(document.getElementById('live').classList.contains('act'))rfImg();},30000);
setInterval(loadSb,10000);
loadSb();
</script>
</body>
</html>
)HTML";

// ---------------------------------------------------------------------------
// WebUI implementation
// ---------------------------------------------------------------------------

void WebUI::begin(config::ConfigManager* config, net::WifiManager* wifi) {
  config_ = config;
  wifi_ = wifi;

  server_.on("/", [this]() { handleRoot(); });
  server_.on("/api/status", HTTP_GET, [this]() { handleApiStatus(); });
  server_.on("/api/preview.bmp", HTTP_GET, [this]() { handleApiPreview(); });
  server_.on("/api/log", HTTP_GET, [this]() { handleApiLog(); });
  server_.on("/api/config", HTTP_POST, [this]() { handleApiConfig(); });
  server_.on("/api/restart", HTTP_POST, [this]() { handleApiRestart(); });
  server_.on("/api/wifi/scan", HTTP_GET, [this]() { handleApiWifiScan(); });

  server_.on("/generate_204", HTTP_GET, [this]() { handleGenerate204(); });      // Android
  server_.on("/gen_204", HTTP_GET, [this]() { handleGenerate204(); });           // Android fallback
  server_.on("/hotspot-detect.html", HTTP_GET, [this]() { handleHotspotDetect(); });  // iOS/macOS
  server_.on("/connecttest.txt", HTTP_GET, [this]() { handleConnectTest(); });   // Windows
  server_.on("/ncsi.txt", HTTP_GET, [this]() { handleConnectTest(); });           // Windows legacy

  server_.onNotFound([this]() { handleCaptiveRedirect(); });

  server_.begin();
  ZO_LOGI("WebUI: HTTP server started on port 80");
}

void WebUI::tick() {
  server_.handleClient();
}

void WebUI::updatePreview(const image::IndexedFramebuffer& framebuffer) {
  lastFramebuffer_ = framebuffer;
  hasPreview_ = true;
}

// ---------------------------------------------------------------------------
// Route: GET /   → serve SPA HTML
// ---------------------------------------------------------------------------
void WebUI::handleRoot() {
  server_.sendHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate"));
  server_.send_P(200, "text/html", kHtmlPage);
}

// ---------------------------------------------------------------------------
// Route: GET /api/status  → JSON with runtime status and system info
// ---------------------------------------------------------------------------
void WebUI::handleApiStatus() {
  if (config_ == nullptr || wifi_ == nullptr) {
    server_.send(503, "application/json", F("{\"error\":\"not ready\"}"));
    return;
  }

  const auto& cfg = config_->get();

  String json;
  json.reserve(512);
  json = F("{");
  json += F("\"wifi\":\"");   json += escapeJson(wifi_->statusText());   json += F("\"");
  json += F(",\"ip\":\"");    json += escapeJson(wifi_->ip());           json += F("\"");
  json += F(",\"rssi\":");    json += String(wifi_->rssi());
  json += F(",\"ssid\":\"");  json += escapeJson(cfg.wifi.ssid);         json += F("\"");
  json += F(",\"portalActive\":"); json += wifi_->isPortalActive() ? F("true") : F("false");
  json += F(",\"apSsid\":\""); json += escapeJson(wifi_->apSsid()); json += F("\"");
  json += F(",\"portalIp\":\""); json += escapeJson(wifi_->portalIp()); json += F("\"");
  json += F(",\"apiKey\":\""); json += escapeJson(cfg.apiKey);           json += F("\"");
  json += F(",\"host\":\"");  json += escapeJson(cfg.server.host);       json += F("\"");
  json += F(",\"httpsEnabled\":"); json += cfg.server.useHttps ? F("true") : F("false");
  json += F(",\"fwVersion\":\"");  json += escapeJson(cfg.versions.fwVersion); json += F("\"");
  json += F(",\"apiVersion\":\""); json += escapeJson(cfg.versions.apiVersion); json += F("\"");
  json += F(",\"buildDate\":\"");  json += escapeJson(String(ZO_BUILD_DATE)); json += F("\"");
  json += F(",\"chip\":\"");       json += escapeJson(diagnostics::chipModel()); json += F("\"");
  json += F(",\"flashSize\":\"");  json += String(diagnostics::flashSizeBytes()); json += F(" B\"");
  json += F(",\"sdk\":\"");        json += escapeJson(diagnostics::sdkVersion()); json += F("\"");
  json += F(",\"uptime\":\"");     json += escapeJson(formatUptime());    json += F("\"");
  json += F(",\"freeHeap\":\"");   json += String(ESP.getFreeHeap()); json += F(" B\"");
  json += '}';

  server_.sendHeader(F("Cache-Control"), F("no-cache"));
  server_.send(200, "application/json", json);
}

// ---------------------------------------------------------------------------
// Route: GET /api/preview.bmp  → 8-bpp Windows BMP from last framebuffer
// ---------------------------------------------------------------------------
void WebUI::handleApiPreview() {
  if (!hasPreview_) {
    server_.send(404, "text/plain", F("No preview available yet"));
    return;
  }

  const uint16_t w = lastFramebuffer_.width();
  const uint16_t h = lastFramebuffer_.height();

  // BMP rows must be padded to a multiple of 4 bytes.
  const uint32_t rowStride = ((uint32_t)w + 3u) & ~3u;
  const uint32_t pixelDataSize = rowStride * h;
  // File layout: file-header(14) + info-header(40) + palette(256*4=1024) + pixels
  const uint32_t totalSize = 14u + 40u + 1024u + pixelDataSize;
  const uint32_t pixelOffset = 14u + 40u + 1024u;

  WiFiClient client = server_.client();
  if (!client) {
    return;
  }

  // --- HTTP response headers (written manually to allow binary body) ---
  String httpHdr;
  httpHdr.reserve(128);
  httpHdr = F("HTTP/1.1 200 OK\r\n"
              "Content-Type: image/bmp\r\n"
              "Cache-Control: no-cache\r\n"
              "Content-Length: ");
  httpHdr += totalSize;
  httpHdr += F("\r\nConnection: close\r\n\r\n");
  client.print(httpHdr);

  // --- BMP file header (14 bytes) ---
  uint8_t fh[14] = {};
  fh[0] = 'B';
  fh[1] = 'M';
  fh[2] = static_cast<uint8_t>(totalSize);
  fh[3] = static_cast<uint8_t>(totalSize >> 8);
  fh[4] = static_cast<uint8_t>(totalSize >> 16);
  fh[5] = static_cast<uint8_t>(totalSize >> 24);
  // fh[6..9] reserved = 0
  fh[10] = static_cast<uint8_t>(pixelOffset);
  fh[11] = static_cast<uint8_t>(pixelOffset >> 8);
  fh[12] = static_cast<uint8_t>(pixelOffset >> 16);
  fh[13] = static_cast<uint8_t>(pixelOffset >> 24);
  client.write(fh, 14);

  // --- BMP info header / BITMAPINFOHEADER (40 bytes) ---
  uint8_t ih[40] = {};
  ih[0] = 40;  // biSize
  ih[4] = static_cast<uint8_t>(w);
  ih[5] = static_cast<uint8_t>(w >> 8);   // biWidth
  ih[8] = static_cast<uint8_t>(h);
  ih[9] = static_cast<uint8_t>(h >> 8);   // biHeight (positive = bottom-up)
  ih[12] = 1;                              // biPlanes
  ih[14] = 8;                              // biBitCount = 8 bpp
  // biCompression = 0 (BI_RGB), biSizeImage = 0, biXPelsPerMeter = 0,
  // biYPelsPerMeter = 0, biClrImportant = 0
  ih[32] = 8;  // biClrUsed – only 8 colors are defined
  client.write(ih, 40);

  // --- Color palette (256 entries × 4 bytes BGRA = 1024 bytes) ---
  uint8_t pal[1024] = {};
  for (uint8_t i = 0; i < 8; ++i) {
    uint8_t r, g, b;
    colorIndexToRgb(i, r, g, b);
    pal[i * 4 + 0] = b;  // BMP palette order: Blue
    pal[i * 4 + 1] = g;  //                    Green
    pal[i * 4 + 2] = r;  //                    Red
    // pal[i * 4 + 3] = 0  (reserved / alpha)
  }
  client.write(pal, 1024);

  // --- Pixel data – BMP stores rows bottom-up ---
  std::vector<uint8_t> row(rowStride, 0);
  for (int32_t y = static_cast<int32_t>(h) - 1; y >= 0; --y) {
    for (uint16_t x = 0; x < w; ++x) {
      uint8_t idx = 0;
      lastFramebuffer_.getPixel(x, static_cast<uint16_t>(y), idx);
      row[x] = idx;
    }
    // Padding bytes (row[w..rowStride-1]) remain 0.
    client.write(row.data(), rowStride);
  }

  client.stop();
}

// ---------------------------------------------------------------------------
// Route: GET /api/log?since=N  → JSON { "lines": [...], "next": N }
// ---------------------------------------------------------------------------
void WebUI::handleApiLog() {
  size_t since = 0;
  if (server_.hasArg("since")) {
    const long v = server_.arg("since").toInt();
    if (v > 0) {
      since = static_cast<size_t>(v);
    }
  }

  size_t next = 0;
  const String json = diagnostics::gLogBuffer.getJson(since, next);

  server_.sendHeader(F("Cache-Control"), F("no-cache"));
  server_.send(200, "application/json", json);
}

// ---------------------------------------------------------------------------
// Route: POST /api/config  (application/x-www-form-urlencoded)
//   Fields: ssid, pass, host, https (0|1)
// ---------------------------------------------------------------------------
void WebUI::handleApiConfig() {
  if (config_ == nullptr || wifi_ == nullptr) {
    server_.send(503, "application/json", F("{\"ok\":false,\"error\":\"not ready\"}"));
    return;
  }

  const String ssid = server_.arg("ssid");
  const String pass = server_.arg("pass");
  const String host = server_.arg("host");
  const bool useHttps = server_.arg("https") == "1";

  if (ssid.isEmpty()) {
    server_.send(400, "application/json", F("{\"ok\":false,\"error\":\"SSID nesmi byt prazdne\"}"));
    return;
  }
  if (ssid.length() > 32 || pass.length() > 64 || host.length() > 128) {
    server_.send(400, "application/json", F("{\"ok\":false,\"error\":\"Neplatna delka vstupu\"}"));
    return;
  }

  bool wifiOk = config_->setWifi(ssid, pass);
  bool serverOk = config_->setServer(host, useHttps);

  if (!wifiOk || !serverOk) {
    server_.send(500, "application/json", F("{\"ok\":false,\"error\":\"NVS save failed\"}"));
    return;
  }

  const bool connected = wifi_->applyConfigAndConnect(config_->get().wifi, 15000);
  String json = F("{\"ok\":true,\"connected\":");
  json += connected ? F("true") : F("false");
  json += F(",\"portalActive\":");
  json += wifi_->isPortalActive() ? F("true") : F("false");
  json += F("}");

  server_.send(200, "application/json", json);
}

// ---------------------------------------------------------------------------
// Route: POST /api/restart  → respond then reboot
// ---------------------------------------------------------------------------
void WebUI::handleApiRestart() {
  server_.send(200, "application/json", F("{\"ok\":true}"));
  delay(200);
  esp_restart();
}


void WebUI::handleApiWifiScan() {
  if (wifi_ == nullptr) {
    server_.send(503, "application/json", F("{\"ok\":false,\"error\":\"wifi not ready\"}"));
    return;
  }

  const int count = WiFi.scanNetworks(false, true);
  if (count < 0) {
    server_.send(500, "application/json", F("{\"ok\":false,\"error\":\"scan failed\"}"));
    return;
  }

  String json;
  json.reserve(1024);
  json = F("{\"ok\":true,\"networks\":[");
  for (int i = 0; i < count; ++i) {
    if (i > 0) {
      json += ',';
    }
    json += F("{\"ssid\":\"");
    json += escapeJson(WiFi.SSID(i));
    json += F("\",\"rssi\":");
    json += WiFi.RSSI(i);
    json += F(",\"secure\":");
    json += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? F("false") : F("true");
    json += '}';
  }
  json += F("]}");

  WiFi.scanDelete();
  server_.send(200, "application/json", json);
}

void WebUI::handleGenerate204() {
  if (wifi_ != nullptr && wifi_->isPortalActive()) {
    server_.sendHeader(F("Location"), String("http://") + wifi_->portalIp() + "/", true);
    server_.send(302, "text/plain", "");
    return;
  }
  server_.send(204, "text/plain", "");
}

void WebUI::handleHotspotDetect() {
  if (wifi_ != nullptr && wifi_->isPortalActive()) {
    server_.sendHeader(F("Location"), String("http://") + wifi_->portalIp() + "/", true);
    server_.send(302, "text/plain", "");
    return;
  }
  server_.send(200, "text/plain", "Success");
}

void WebUI::handleConnectTest() {
  if (wifi_ != nullptr && wifi_->isPortalActive()) {
    server_.sendHeader(F("Location"), String("http://") + wifi_->portalIp() + "/", true);
    server_.send(302, "text/plain", "");
    return;
  }
  server_.send(200, "text/plain", "Microsoft Connect Test");
}

void WebUI::handleCaptiveRedirect() {
  if (wifi_ != nullptr && wifi_->isPortalActive()) {
    server_.sendHeader(F("Location"), String("http://") + wifi_->portalIp() + "/", true);
    server_.send(302, "text/plain", "");
    return;
  }
  server_.send(404, "text/plain", "Not found");
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void WebUI::colorIndexToRgb(uint8_t index, uint8_t& r, uint8_t& g, uint8_t& b) {
  // Matches the mapping in St7789Display::mapColorIndex().
  switch (index) {
    case 0: r = 255; g = 255; b = 255; return;  // WHITE
    case 1: r = 0;   g = 0;   b = 0;   return;  // BLACK
    case 2: r = 248; g = 0;   b = 0;   return;  // RED   (RGB565 0xF800)
    case 3: r = 248; g = 252; b = 0;   return;  // YELLOW(RGB565 0xFFE0)
    case 4: r = 0;   g = 248; b = 0;   return;  // GREEN (RGB565 0x07E0)
    case 5: r = 0;   g = 0;   b = 248; return;  // BLUE  (RGB565 0x001F)
    case 6: r = 255; g = 165; b = 0;   return;  // ORANGE
    default: r = 0; g = 0; b = 0; return;        // BLACK (fallback)
  }
}

String WebUI::formatUptime() {
  const uint32_t totalSec = millis() / 1000u;
  const uint32_t sec = totalSec % 60u;
  const uint32_t totalMin = totalSec / 60u;
  const uint32_t min = totalMin % 60u;
  const uint32_t totalHr = totalMin / 60u;
  const uint32_t hr = totalHr % 24u;
  const uint32_t day = totalHr / 24u;

  char buf[32];
  if (day > 0) {
    snprintf(buf, sizeof(buf), "%ud %uh %um", (unsigned)day, (unsigned)hr, (unsigned)min);
  } else if (hr > 0) {
    snprintf(buf, sizeof(buf), "%uh %um", (unsigned)hr, (unsigned)min);
  } else {
    snprintf(buf, sizeof(buf), "%um %us", (unsigned)min, (unsigned)sec);
  }
  return String(buf);
}

String WebUI::escapeJson(const String& s) {
  String out;
  out.reserve(s.length() + 4);
  for (size_t i = 0; i < s.length(); ++i) {
    const char c = s[i];
    if (c == '"')       { out += F("\\\""); }
    else if (c == '\\') { out += F("\\\\"); }
    else if (c == '\n') { out += F("\\n");  }
    else if (c == '\r') { /* skip */        }
    else                { out += c;         }
  }
  return out;
}

}  // namespace zivyobraz::web
