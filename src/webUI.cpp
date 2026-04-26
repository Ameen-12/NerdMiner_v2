#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "webUI.h"
#include "drivers/storage/storage.h"
#include "drivers/storage/nvMemory.h"

// Access mining stats from mining.cpp
extern uint32_t templates;
extern uint32_t hashes;
extern uint32_t Mhashes;
extern uint32_t totalKHashes;
extern uint32_t elapsedKHs;
extern uint64_t upTime;
extern volatile uint32_t shares;
extern volatile uint32_t valids;
extern double best_diff;
extern TSettings Settings;

static WebServer server(80);
static nvMemory nvMem2;

// ─── HTML Dashboard Page ────────────────────────────────────────────────────

static const char DASHBOARD_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>NerdMiner Dashboard</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: 'Segoe UI', Arial, sans-serif;
    background: #0d0d0d;
    color: #e0e0e0;
    min-height: 100vh;
    padding: 20px;
  }
  h1 {
    text-align: center;
    color: #f7931a;
    font-size: 1.8em;
    margin-bottom: 6px;
    letter-spacing: 2px;
  }
  .subtitle {
    text-align: center;
    color: #666;
    font-size: 0.85em;
    margin-bottom: 24px;
  }
  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
    gap: 14px;
    margin-bottom: 28px;
  }
  .card {
    background: #1a1a1a;
    border: 1px solid #2a2a2a;
    border-radius: 10px;
    padding: 16px 12px;
    text-align: center;
  }
  .card .label {
    font-size: 0.72em;
    color: #888;
    text-transform: uppercase;
    letter-spacing: 1px;
    margin-bottom: 8px;
  }
  .card .value {
    font-size: 1.6em;
    font-weight: bold;
    color: #f7931a;
  }
  .card .unit {
    font-size: 0.75em;
    color: #666;
    margin-top: 2px;
  }
  .status-dot {
    display: inline-block;
    width: 10px; height: 10px;
    border-radius: 50%;
    margin-right: 6px;
  }
  .dot-green { background: #22c55e; box-shadow: 0 0 6px #22c55e; }
  .dot-red   { background: #ef4444; }
  .section-title {
    color: #f7931a;
    font-size: 1em;
    font-weight: bold;
    margin-bottom: 14px;
    padding-bottom: 6px;
    border-bottom: 1px solid #2a2a2a;
  }
  .settings-box {
    background: #1a1a1a;
    border: 1px solid #2a2a2a;
    border-radius: 10px;
    padding: 20px;
    margin-bottom: 20px;
  }
  label {
    display: block;
    font-size: 0.8em;
    color: #888;
    margin-bottom: 4px;
    margin-top: 14px;
  }
  input[type=text], input[type=number], input[type=password] {
    width: 100%;
    background: #0d0d0d;
    border: 1px solid #333;
    border-radius: 6px;
    color: #e0e0e0;
    padding: 9px 12px;
    font-size: 0.95em;
    outline: none;
  }
  input:focus { border-color: #f7931a; }
  .btn {
    display: inline-block;
    margin-top: 18px;
    padding: 10px 28px;
    background: #f7931a;
    color: #000;
    border: none;
    border-radius: 6px;
    font-size: 0.95em;
    font-weight: bold;
    cursor: pointer;
    width: 100%;
  }
  .btn:hover { background: #e07d0a; }
  .btn-danger {
    background: #ef4444;
    color: #fff;
    margin-top: 10px;
  }
  .btn-danger:hover { background: #b91c1c; }
  .msg {
    text-align: center;
    padding: 10px;
    border-radius: 6px;
    margin-top: 12px;
    font-size: 0.9em;
    display: none;
  }
  .msg-ok  { background: #14532d; color: #86efac; }
  .msg-err { background: #450a0a; color: #fca5a5; }
  .uptime-bar {
    text-align: center;
    color: #555;
    font-size: 0.8em;
    margin-top: 10px;
  }
</style>
</head>
<body>

<h1>&#9889; NerdMiner</h1>
<p class="subtitle">ESP32 Bitcoin Solo Miner &nbsp;&bull;&nbsp; <span id="ip"></span></p>

<!-- Stats Grid -->
<div class="grid" id="statsGrid">
  <div class="card">
    <div class="label">Hashrate</div>
    <div class="value" id="hashrate">--</div>
    <div class="unit">KH/s</div>
  </div>
  <div class="card">
    <div class="label">Shares</div>
    <div class="value" id="shares">--</div>
    <div class="unit">accepted</div>
  </div>
  <div class="card">
    <div class="label">Best Diff</div>
    <div class="value" id="bestdiff">--</div>
    <div class="unit">&nbsp;</div>
  </div>
  <div class="card">
    <div class="label">Templates</div>
    <div class="value" id="templates">--</div>
    <div class="unit">jobs received</div>
  </div>
  <div class="card">
    <div class="label">Valid Blocks</div>
    <div class="value" id="valids">--</div>
    <div class="unit">found</div>
  </div>
  <div class="card">
    <div class="label">Pool</div>
    <div class="value" id="pool" style="font-size:0.85em; word-break:break-all;">--</div>
    <div class="unit">&nbsp;</div>
  </div>
</div>

<div class="uptime-bar">Uptime: <span id="uptime">--</span></div>

<br>

<!-- Settings -->
<div class="settings-box">
  <div class="section-title">&#9881; Settings</div>
  <form id="settingsForm">
    <label>Bitcoin Wallet Address</label>
    <input type="text" id="wallet" name="wallet" maxlength="100" placeholder="bc1q...">

    <label>Pool URL</label>
    <input type="text" id="poolurl" name="poolurl" maxlength="80" placeholder="public-pool.io">

    <label>Pool Port</label>
    <input type="number" id="poolport" name="poolport" min="1" max="65535" placeholder="21496">

    <label>Pool Password (optional)</label>
    <input type="password" id="poolpass" name="poolpass" maxlength="80" placeholder="x">

    <button type="submit" class="btn">&#128190; Save &amp; Restart</button>
    <button type="button" class="btn btn-danger" onclick="resetDevice()">&#128465; Factory Reset</button>
  </form>
  <div class="msg msg-ok"  id="msgOk">&#10003; Saved! Restarting...</div>
  <div class="msg msg-err" id="msgErr">&#10007; Error saving settings.</div>
</div>

<script>
// Fill IP
document.getElementById('ip').textContent = location.hostname;

// Format uptime seconds → d h m s
function fmtUptime(s) {
  const d = Math.floor(s/86400), h = Math.floor((s%86400)/3600),
        m = Math.floor((s%3600)/60), sec = s%60;
  let r = '';
  if (d) r += d+'d ';
  if (h) r += h+'h ';
  if (m) r += m+'m ';
  r += sec+'s';
  return r;
}

// Format diff
function fmtDiff(v) {
  if (v >= 1e12) return (v/1e12).toFixed(2)+'T';
  if (v >= 1e9)  return (v/1e9).toFixed(2)+'G';
  if (v >= 1e6)  return (v/1e6).toFixed(2)+'M';
  if (v >= 1e3)  return (v/1e3).toFixed(2)+'K';
  return v.toFixed(2);
}

// Fetch stats every 5 seconds
function fetchStats() {
  fetch('/api/stats')
    .then(r => r.json())
    .then(d => {
      document.getElementById('hashrate').textContent  = d.hashrate.toFixed(2);
      document.getElementById('shares').textContent    = d.shares;
      document.getElementById('bestdiff').textContent  = fmtDiff(d.best_diff);
      document.getElementById('templates').textContent = d.templates;
      document.getElementById('valids').textContent    = d.valids;
      document.getElementById('pool').textContent      = d.pool;
      document.getElementById('uptime').textContent    = fmtUptime(d.uptime);
    })
    .catch(() => {});
}

// Load current settings into form
function loadSettings() {
  fetch('/api/settings')
    .then(r => r.json())
    .then(d => {
      document.getElementById('wallet').value   = d.wallet   || '';
      document.getElementById('poolurl').value  = d.poolurl  || '';
      document.getElementById('poolport').value = d.poolport || '';
      document.getElementById('poolpass').value = d.poolpass || '';
    })
    .catch(() => {});
}

// Save settings
document.getElementById('settingsForm').addEventListener('submit', function(e) {
  e.preventDefault();
  const body = JSON.stringify({
    wallet:   document.getElementById('wallet').value,
    poolurl:  document.getElementById('poolurl').value,
    poolport: parseInt(document.getElementById('poolport').value),
    poolpass: document.getElementById('poolpass').value
  });
  fetch('/api/save', { method:'POST', headers:{'Content-Type':'application/json'}, body })
    .then(r => {
      if (r.ok) {
        document.getElementById('msgOk').style.display='block';
        setTimeout(() => location.reload(), 4000);
      } else {
        document.getElementById('msgErr').style.display='block';
      }
    })
    .catch(() => { document.getElementById('msgErr').style.display='block'; });
});

// Factory reset
function resetDevice() {
  if (!confirm('Are you sure? This will erase all settings and restart.')) return;
  fetch('/api/reset', { method:'POST' });
  alert('Resetting device...');
  setTimeout(() => location.reload(), 5000);
}

// Init
fetchStats();
loadSettings();
setInterval(fetchStats, 5000);
</script>
</body>
</html>
)rawhtml";

// ─── API Handlers ────────────────────────────────────────────────────────────

void handleRoot() {
  server.send_P(200, "text/html", DASHBOARD_HTML);
}

void handleApiStats() {
  // Calculate hashrate in KH/s
  float hashrate = (float)elapsedKHs;

  // Format uptime
  uint64_t ut = upTime;

  char json[512];
  snprintf(json, sizeof(json),
    "{\"hashrate\":%.2f,\"shares\":%lu,\"valids\":%lu,"
    "\"best_diff\":%.2f,\"templates\":%lu,\"uptime\":%llu,"
    "\"pool\":\"%s\"}",
    hashrate,
    (unsigned long)shares,
    (unsigned long)valids,
    best_diff,
    (unsigned long)templates,
    ut,
    Settings.PoolAddress.c_str()
  );
  server.send(200, "application/json", json);
}

void handleApiSettings() {
  char json[512];
  snprintf(json, sizeof(json),
    "{\"wallet\":\"%s\",\"poolurl\":\"%s\",\"poolport\":%d,\"poolpass\":\"%s\"}",
    Settings.BtcWallet,
    Settings.PoolAddress.c_str(),
    Settings.PoolPort,
    Settings.PoolPassword
  );
  server.send(200, "application/json", json);
}

void handleApiSave() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"no body\"}");
    return;
  }

  String body = server.arg("plain");

  // Simple JSON parsing without ArduinoJson to save memory
  // Extract wallet
  int idx;
  auto extractStr = [&](const char* key) -> String {
    String k = String("\"") + key + "\":\"";
    idx = body.indexOf(k);
    if (idx < 0) return "";
    idx += k.length();
    int end = body.indexOf("\"", idx);
    if (end < 0) return "";
    return body.substring(idx, end);
  };
  auto extractInt = [&](const char* key) -> int {
    String k = String("\"") + key + "\":";
    idx = body.indexOf(k);
    if (idx < 0) return 0;
    idx += k.length();
    int end = idx;
    while (end < (int)body.length() && (isDigit(body[end]) || body[end]=='-')) end++;
    return body.substring(idx, end).toInt();
  };

  String wallet  = extractStr("wallet");
  String poolurl = extractStr("poolurl");
  int    port    = extractInt("poolport");
  String poolpass = extractStr("poolpass");

  if (wallet.length() > 0)  strncpy(Settings.BtcWallet,    wallet.c_str(),   sizeof(Settings.BtcWallet)-1);
  if (poolurl.length() > 0) Settings.PoolAddress = poolurl;
  if (port > 0)             Settings.PoolPort = port;
  strncpy(Settings.PoolPassword, poolpass.c_str(), sizeof(Settings.PoolPassword)-1);

  nvMem2.saveConfig(&Settings);

  server.send(200, "application/json", "{\"ok\":true}");

  // Restart after short delay
  delay(500);
  ESP.restart();
}

void handleApiReset() {
  server.send(200, "application/json", "{\"ok\":true}");
  delay(500);
  nvMem2.deleteConfig();
  ESP.restart();
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ─── Public API ──────────────────────────────────────────────────────────────

void startWebUI() {
  if (WiFi.status() != WL_CONNECTED) return;

  server.on("/",            HTTP_GET,  handleRoot);
  server.on("/api/stats",   HTTP_GET,  handleApiStats);
  server.on("/api/settings",HTTP_GET,  handleApiSettings);
  server.on("/api/save",    HTTP_POST, handleApiSave);
  server.on("/api/reset",   HTTP_POST, handleApiReset);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.printf("[WebUI] Started at http://%s\n", WiFi.localIP().toString().c_str());
}

void handleWebUI() {
  server.handleClient();
}
