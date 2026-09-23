// =============================================================================
//  Flock Noir  -  web_ui.h   (GENERATED from web/index.html - do not hand edit)
//  Single-page UI. Tabs: ALPR / Scanner / Camera / Wardrive / Settings.
//  Polls /api/status ~2x/sec. Logo served UNALTERED at /logo.png.
//  Regenerate with:  python tools/html2header.py
// =============================================================================
#pragma once
#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"HTMLPAGE(
<!doctype html><html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Flock Noir</title>
<style>
:root{
 --bg:#04070a; --panel:#0a1016; --panel2:#0d151c; --line:#15242e;
 --ink:#d8e6ec; --mut:#5f7681; --green:#3dfba0; --green-d:#1f7d54;
 --amber:#ffcb57; --red:#ff4d5e; --blue:#57b6ff;
 --mono:'JetBrains Mono','SFMono-Regular',ui-monospace,Menlo,Consolas,monospace;
}
*{box-sizing:border-box}
html,body{margin:0}
body{background:radial-gradient(120% 90% at 50% -10%,#0b1620 0%,var(--bg) 55%);
 color:var(--ink);font:14px/1.5 var(--mono);min-height:100vh;-webkit-font-smoothing:antialiased}
body::after{content:"";position:fixed;inset:0;pointer-events:none;z-index:9;
 background:repeating-linear-gradient(0deg,rgba(0,0,0,.16) 0 1px,transparent 1px 3px);opacity:.30}
a{color:var(--green)}
.wrap{max-width:900px;margin:0 auto;padding:0 16px 40px}

/* ---- top bar ---- */
header{display:flex;align-items:center;gap:16px;padding:16px 16px 12px;max-width:900px;margin:0 auto}
.brand-plate{flex:0 0 auto;background:radial-gradient(120% 120% at 50% 30%,#fbfdff 0%,#e8eef2 60%,#c9d3da 100%);
 border-radius:12px;padding:8px 12px;display:flex;align-items:center;justify-content:center;
 box-shadow:0 0 0 1px rgba(61,251,160,.35),0 0 24px rgba(61,251,160,.16),inset 0 1px 0 rgba(255,255,255,.7)}
.brand-plate img{height:52px;display:block}
.brand-txt{display:flex;flex-direction:column;line-height:1.15}
.brand-txt .co{font-size:20px;font-weight:800;letter-spacing:3px}
.brand-txt .pr{font-size:20px;font-weight:800;letter-spacing:3px;color:var(--green);
 text-shadow:0 0 14px rgba(61,251,160,.45)}
.brand-txt .pr .cur{animation:blink 1.1s steps(1) infinite}
.brand-txt .tag{font-size:11px;color:var(--mut);letter-spacing:1px;margin-top:3px}
.status{margin-left:auto;text-align:right;font-size:11px;color:var(--mut);letter-spacing:.5px}
.status .led{display:inline-block;width:9px;height:9px;border-radius:50%;background:var(--green-d);
 box-shadow:0 0 8px var(--green);margin-right:6px;vertical-align:middle}
.status .ip{color:var(--ink)}
@keyframes blink{50%{opacity:0}}

/* ---- tabs ---- */
.tabs{display:flex;flex-wrap:wrap;gap:4px;border-bottom:1px solid var(--line);max-width:900px;margin:6px auto 0;padding:0 16px}
.tab{appearance:none;background:none;border:0;color:var(--mut);font:600 12px var(--mono);letter-spacing:1.5px;
 padding:12px 16px;cursor:pointer;border-bottom:2px solid transparent;margin-bottom:-1px}
.tab:hover{color:var(--ink)}
.tab.on{color:var(--green);border-bottom-color:var(--green)}
@media(max-width:480px){
 .tab{padding:12px 10px;font-size:11px}
 .row-head{flex-wrap:wrap;gap:6px}
 header{flex-wrap:wrap;gap:12px}
 .brand-txt{min-width:0;flex:1}
 .brand-txt .tag{overflow-wrap:anywhere}
 .status{flex-basis:100%;display:flex;justify-content:space-between;gap:8px;text-align:left}
 .grid>.field{min-width:0}
}

/* ---- cards / tiles ---- */
.card{background:linear-gradient(180deg,var(--panel2),var(--panel));border:1px solid var(--line);
 border-radius:12px;padding:16px;margin-top:16px}
.banner{border-radius:12px;padding:20px;margin-top:16px;border:1px solid var(--line);
 background:linear-gradient(180deg,var(--panel2),var(--panel));font-weight:700;letter-spacing:.5px;
 display:flex;align-items:center;gap:12px}
.banner .ico{font-size:22px}
.banner>span{min-width:0;overflow-wrap:anywhere}
.banner.clear{color:var(--green)}
.banner.clear .ico{color:var(--green-d)}
.banner.alert{background:linear-gradient(180deg,#2a0d12,#1a070b);border-color:var(--red);color:#ffd7db;
 animation:alarm 1s infinite}
@keyframes alarm{0%,100%{box-shadow:0 0 0 0 rgba(255,77,94,.55)}50%{box-shadow:0 0 0 10px rgba(255,77,94,0)}}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px;margin-top:16px}
.tile{background:linear-gradient(180deg,var(--panel2),var(--panel));border:1px solid var(--line);
 border-radius:10px;padding:13px 14px}
.tile .k{color:var(--mut);font-size:10px;letter-spacing:1.5px;text-transform:uppercase}
.tile .v{font-size:22px;font-weight:700;margin-top:5px}
.tile .v small{font-size:11px;color:var(--mut);font-weight:400;letter-spacing:.5px}
.tile.pos .v{font-size:14px}
.bar{height:6px;border-radius:6px;background:#071019;overflow:hidden;margin-top:9px;border:1px solid var(--line)}
.bar>i{display:block;height:100%;width:0;background:linear-gradient(90deg,var(--green-d),var(--green),var(--amber),var(--red));
 transition:width .3s}
.ok{color:var(--green)}.no{color:var(--red)}.dim{color:var(--mut)}

table{width:100%;border-collapse:collapse;font-size:12.5px;margin-top:4px}
th,td{text-align:left;padding:8px 6px;border-bottom:1px solid var(--line)}
th{color:var(--mut);font-weight:600;letter-spacing:.5px;text-transform:uppercase;font-size:10px}
td.right,th.right{text-align:right}
.row-head{display:flex;align-items:center;margin-bottom:6px}
.row-head strong{letter-spacing:1px;font-size:12px;color:var(--mut);text-transform:uppercase}

.btn{appearance:none;display:inline-block;padding:8px 14px;border:1px solid var(--line);border-radius:8px;
 color:var(--ink);text-decoration:none;background:var(--panel2);font:600 12px var(--mono);letter-spacing:.5px;cursor:pointer}
.btn:hover{border-color:var(--green);color:var(--green)}
.btn.primary{background:var(--green);color:#04120b;border-color:var(--green)}
.btn.primary:hover{filter:brightness(1.1);color:#04120b}
.btn.ghost{background:none}
.btn.sm{padding:5px 10px;font-size:11px}

/* ---- settings ---- */
.field{margin:14px 0}
.field label{display:block;color:var(--mut);font-size:11px;letter-spacing:1px;text-transform:uppercase;margin-bottom:6px}
input[type=text],input[type=number],select,textarea{width:100%;background:#060c11;border:1px solid var(--line);border-radius:8px;
 color:var(--ink);font:13px var(--mono);padding:9px 10px}
input[type=text]:focus,input[type=number]:focus,select:focus,textarea:focus{outline:none;border-color:var(--green)}
textarea{resize:vertical;min-height:52px}
.switch{position:relative;display:inline-block;width:48px;height:26px}
.switch input{display:none}
.slider{position:absolute;inset:0;background:#0e1a22;border:1px solid var(--line);border-radius:26px;transition:.2s}
.slider:before{content:"";position:absolute;height:18px;width:18px;left:3px;top:3px;background:var(--mut);border-radius:50%;transition:.2s}
.switch input:checked+.slider{background:rgba(61,251,160,.18);border-color:var(--green)}
.switch input:checked+.slider:before{transform:translateX(22px);background:var(--green)}
.tone{border:1px solid var(--line);border-radius:10px;padding:12px;margin-top:12px;background:#070d12}
.tone .thead{display:flex;align-items:center;gap:10px;margin-bottom:8px}
.tone .thead .idx{color:var(--mut);font-size:11px}
.tone .thead .grow{flex:1}
.radio{display:inline-flex;align-items:center;gap:6px;color:var(--mut);font-size:11px;cursor:pointer}
.radio input{accent-color:var(--green)}
.presets{display:flex;flex-wrap:wrap;gap:8px;margin-top:10px}
.chip{border:1px dashed var(--line);border-radius:20px;padding:5px 12px;font-size:11px;color:var(--mut);cursor:pointer;background:none}
.chip:hover{border-color:var(--green);color:var(--green)}
.hint{color:var(--mut);font-size:11.5px;line-height:1.6;margin-top:10px}
.hint code{color:var(--amber)}
.toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%) translateY(30px);opacity:0;
 background:var(--green);color:#04120b;font-weight:700;padding:10px 18px;border-radius:8px;transition:.25s;z-index:20}
.toast.show{opacity:1;transform:translateX(-50%) translateY(0)}

footer{max-width:900px;margin:26px auto 0;padding:16px;border-top:1px solid var(--line);
 display:flex;align-items:center;flex-wrap:wrap;gap:8px;color:var(--mut);font-size:11.5px}
footer .sig{color:var(--ink);font-style:italic;letter-spacing:.5px}
footer .sig b{color:var(--green);font-style:normal}
footer .spacer{flex:1}
/* ---- live camera view ---- */
.cam-wrap{position:relative;margin-top:14px;max-width:560px;background:#000;border:1px solid var(--line);
 border-radius:10px;overflow:hidden;aspect-ratio:4/3;display:flex;align-items:center;justify-content:center}
.cam-wrap #cam{width:100%;height:100%;object-fit:contain;image-rendering:auto;display:block}
.cam-wrap #cam:not([src]){opacity:0}
.cam-hud{position:absolute;top:0;left:0;right:0;display:flex;gap:8px;padding:8px 10px;font-size:11px;
 letter-spacing:1.5px;color:var(--green);pointer-events:none;
 background:linear-gradient(180deg,rgba(0,0,0,.55),transparent)}
.cam-hud .grow{flex:1}
.cam-mark{position:absolute;right:8px;bottom:8px;padding:3px 5px;border-radius:6px;opacity:.9;
 background:radial-gradient(120% 120% at 50% 40%,rgba(246,249,251,.9),rgba(219,227,233,.72));
 box-shadow:0 0 0 1px rgba(61,251,160,.35),0 1px 5px rgba(0,0,0,.5)}
.cam-mark img{height:18px;display:block}
.rec-bar{display:flex;align-items:center;gap:12px;margin-top:14px}
#recBtn.on{background:var(--red);border-color:var(--red);color:#fff;animation:alarm 1.2s infinite}
.recfile{display:flex;align-items:center;gap:10px;padding:7px 0;border-bottom:1px solid var(--line);color:var(--ink)}
.recfile a{margin-left:auto}
[hidden]{display:none!important}
</style></head><body>

<header>
  <div class="brand-plate"><img src="/logo.png" alt="Flock Noir"
       onerror="this.parentNode.innerHTML='<span style=&quot;color:#04120b;font-weight:800;letter-spacing:2px&quot;>VTS</span>'"></div>
  <div class="brand-txt">
    <span class="co">FLOCK</span>
    <span class="pr">NOIR<span class="cur">_</span></span>
  </div>
  <div class="status">
    <div><span class="led" id="led"></span><span id="clock">--:--:--</span></div>
    <div class="ip">http://192.168.4.1</div>
  </div>
</header>

<div class="tabs">
  <button class="tab on" data-tab="detector">ALPR</button>
  <button class="tab" data-tab="scanner">SCANNER</button>
  <button class="tab" data-tab="camera">CAMERA</button>
  <button class="tab" data-tab="wardrive">WARDRIVE</button>
  <button class="tab" data-tab="settings">SETTINGS</button>
</div>

<div class="wrap">
  <!-- ============ DETECTOR ============ -->
  <section id="detector">
    <div id="banner" class="banner clear">
      <span id="bannerTxt">SCANNING FOR ALPR EVIDENCE...</span></div>

    <div style="display:flex;align-items:center;gap:16px;margin-top:10px;font-size:11px;color:var(--mut);flex-wrap:wrap">
      <label class="radio" style="color:var(--ink)"><input type="checkbox" id="mute"> mute alerts</label>
      <span id="sysInfo"></span>
    </div>

    <div class="grid">
      <div class="tile"><div class="k">Signal</div><div class="v" id="freq">--<small> Hz</small></div>
        <div class="bar"><i id="conf"></i></div></div>
      <div class="tile"><div class="k">Duty Cycle</div><div class="v" id="duty">--<small> %</small></div></div>
      <div class="tile"><div class="k">GPS Fix</div><div class="v" id="fix">--</div></div>
      <div class="tile"><div class="k">Satellites</div><div class="v" id="sats">--</div></div>
      <div class="tile pos"><div class="k">Position</div><div class="v" id="pos">no fix</div></div>
      <div class="tile"><div class="k">Camera</div><div class="v" id="fps">--<small> fps</small></div></div>
      <div class="tile"><div class="k">microSD</div><div class="v" id="sd">--</div></div>
      <div class="tile"><div class="k">Detections</div><div class="v" id="count">0</div></div>
    </div>

    <div class="card" id="fusionCard">
      <div class="row-head"><strong>ALPR focus</strong><span style="flex:1"></span><button class="btn primary" id="alprProfile">FOCUS ALPR</button></div>
      <div class="hint" id="profileState"></div>
      <div class="grid">
        <div class="tile"><div class="k">OPT101 timing</div><div id="fusionIr">Waiting</div><div class="hint" id="readyIr"></div></div>
        <div class="tile"><div class="k">Camera pattern</div><div id="fusionCamera">Waiting</div><div class="hint" id="readyCamera"></div></div>
        <div class="tile"><div class="k">Flock BLE</div><div id="fusionBle">Waiting</div><div class="hint" id="readyBle"></div></div>
        <div class="tile"><div class="k">Flock WiFi / OUI</div><div id="fusionWifi">Waiting</div><div class="hint" id="readyWifi"></div></div>
      </div>
      <div class="hint" id="fusionSummary">Waiting for evidence...</div>
      <div class="hint">Evidence is combined within 3 seconds. Multiple sources increase support for a camera candidate; nearby radio and light may come from different devices. No recent evidence does not rule out a camera.</div>
      <div class="hint">Focus enables BLE and selects priority channels 1/6/11 for field mode, with ALPR-only automatic sounds. All device observations still log. Enable OPT101 below after wiring; camera acquisition continues automatically. The radio shares time between WiFi and BLE.</div>
    </div>

    <div class="card" id="detectorRadioCard" hidden>
      <div class="row-head"><strong>ALPR radio candidates</strong><span style="flex:1"></span>
        <a class="btn sm" href="/api/log" download>CSV</a>
        <a class="btn sm" href="/api/radio/log" download>EVENT LOG</a></div>
      <div class="hint" id="detectorRadioHealth"></div>
      <div style="overflow-x:auto"><table><thead><tr><th>Device / name</th><th>Evidence</th><th>RSSI</th><th>Age</th></tr></thead>
        <tbody id="detectorRadioRows"></tbody></table></div>
      <div class="hint">Flock-You OUI, probe and BLE rules run alongside optical detection. Other device types are in Scanner. Tier 1 is a weak hint; signatures can be shared or spoofed.</div>
    </div>

    <div class="card">
      <div class="row-head"><strong>Signal scope (live)</strong>
        <span style="flex:1"></span>
        <span class="dim" id="sigMeta" style="font-size:11px"></span></div>
      <canvas id="wave" width="880" height="90"
        style="width:100%;height:90px;display:block;background:#060c11;border:1px solid var(--line);border-radius:8px"></canvas>
      <div class="hint" style="margin-top:8px">Brightness of the brightest spot in view. A flat scope can mean no visible pulse,
        poor aim, saturation or missed flashes between frames. The OPT101 measures faster pulses independently;
        a camera pattern alone cannot identify an ALPR.</div>
    </div>

    <div class="card">
      <div class="row-head"><strong>IR photodiode sensor</strong>
        <span style="flex:1"></span>
        <span class="dim" id="irMeta" style="font-size:11px;margin-right:12px"></span>
        <label class="switch"><input type="checkbox" id="irEn"><span class="slider"></span></label></div>
      <canvas id="irwave" width="880" height="70"
        style="width:100%;height:70px;display:block;background:#060c11;border:1px solid var(--line);border-radius:8px"></canvas>
      <div class="hint" style="margin-top:8px">OPT101 pulse timing: VCC to <b>3V3</b>, GND to <b>GND</b>, OUT to
        <b>D1 (GPIO2)</b> on the XIAO. Pi builds use MCP3008 CH0. See <a href="https://github.com/valleytechsolutions/Flock-Noir/blob/main/HARDWARE.md">HARDWARE.md</a> -
        then flip this on. Status: <b id="irStat">off</b>.</div>
      <div class="hint" id="irHealth"></div>
      <div class="hint">Timing profile: <b>8–12 Hz</b>, <b>10–30% duty</b>, <b>8–35 ms pulses</b>,
        four consecutive matching intervals at a target 1,000 samples/sec. Optical and radio evidence within three
        seconds is logged together; co-occurrence does not prove they are the same device.</div>
    </div>

    <div class="card">
      <div class="row-head"><strong>Recent detections</strong>
        <span class="spacer" style="flex:1"></span>
        <a class="btn sm" href="/api/log" download>CSV</a></div>
      <div style="overflow-x:auto"><table><thead><tr><th>Time (UTC)</th><th>Src</th><th>Lat</th><th>Lon</th>
        <th class="right">Hz</th><th class="right">Duty</th><th class="right">Conf</th></tr></thead>
        <tbody id="rows"><tr><td colspan="7" class="dim">no detections logged yet</td></tr></tbody></table></div>
    </div>
  </section>

  <!-- ============ CAMERA ============ -->
  <section id="camera" hidden>
    <div class="card">
      <div class="row-head"><strong>Live NIR view</strong><span style="flex:1"></span>
        <button class="btn primary" id="camBtn">START LIVE VIEW</button></div>
      <div class="hint" style="margin-top:0">Near-IR view (640×480 on XIAO) with fixed exposure <i>for detection</i>. This is
        a <b>near-infrared view</b> - bright blobs are IR / light sources. Point it at a suspected ALPR and watch
        for a fast-flickering hot spot. Refreshes a few frames/sec so detection keeps running.</div>
      <div class="cam-wrap" id="camWrap">
        <img id="cam" alt="">
        <div class="cam-hud"><span id="camState">OFFLINE</span><span class="grow"></span><span id="camFps"></span></div>
        <div class="cam-mark"><img src="/logo.png" alt="VTS"></div>
      </div>

      <div class="rec-bar">
        <button class="btn" id="recBtn">REC</button>
        <label class="radio" style="color:var(--ink)"><input type="checkbox" id="recAudio"> with audio</label>
        <span id="recStat" class="dim" style="margin-left:auto"></span>
      </div>
      <div class="hint" style="margin-top:8px">Records to SD as MJPEG <b>AVI</b> (plays in VLC). "With audio" also
        writes a matching <b>WAV</b> from the onboard mic (same name) - combine them in any editor. Recording
        is the NIR view, same as above.</div>

      <div class="row-head" style="margin-top:14px"><strong>Recordings</strong>
        <span style="flex:1"></span><button class="btn sm ghost" id="recsRefresh">refresh</button></div>
      <div id="recsList" class="hint">-</div>
    </div>
  </section>

  <section id="scanner" hidden>
    <div class="banner clear" id="generalBanner"><span id="generalBannerTxt">SCANNING FOR DEVICE SIGNATURES...</span></div>
    <div class="card">
      <div class="row-head"><strong>General scan</strong><span style="flex:1"></span><button class="btn primary" id="generalProfile">USE GENERAL ALERTS</button></div>
      <div class="hint" id="generalProfileState"></div>
      <div class="hint">ALPR / Flock, Axon, Ring, Meta glasses, Flipper Zero, WiFi Pineapple, Biscuit and drone signatures. Each has an editable sound in Settings.</div>
      <div class="hint">Pineapple Pager is covered only when it advertises a recognizable Pineapple-family network name. This cannot identify the Pager model, silent devices or renamed networks. Biscuit uses its advertised name; matching name + service is stronger evidence.</div>
      <div class="field"><label for="deviceFilter">Show devices</label><select id="deviceFilter"><option value="all">All matches</option><option value="alpr">ALPR / Flock</option><option value="axon">Axon</option><option value="ring">Ring</option><option value="meta">Meta</option><option value="flipper">Flipper</option><option value="pineapple">Pineapple family / Pager candidates</option><option value="biscuit">Biscuit</option><option value="drone">Drones</option></select></div>
      <a class="btn sm" href="/api/log" download>DETECTION CSV</a>
    </div>
    <div class="card" id="radioCard" hidden>
      <div class="row-head"><strong>Radio intelligence</strong><span style="flex:1"></span><span id="radioHealth" class="dim"></span></div>
      <div class="hint" id="radioModeHint">Passive WiFi and BLE signatures, drone Remote ID, and IR evidence in one session.
        <b>Field mode turns off the hotspot</b> and hops the selected channels. Hold <b>BOOT for 1.5 seconds</b>
        to restore this dashboard. IR sampling continues in both modes.</div>
      <div class="grid">
        <div class="field"><label for="radioMode">Radio mode</label><select id="radioMode"><option value="dashboard">Dashboard</option><option value="field">Field / channel hopping</option></select></div>
        <div class="field"><label for="radioChannel">Dashboard channel (1-11)</label><input id="radioChannel" type="number" min="1" max="11" value="1"></div>
        <div class="field"><label for="radioHop">Field scan plan</label><select id="radioHop"><option value="priority">Flock-You priority: 1 / 6 / 11</option><option value="all">All channels: 1–11</option></select></div>
      </div>
      <div style="display:flex;gap:18px;margin-top:12px;flex-wrap:wrap">
        <label><input type="checkbox" id="radioBle" checked> BLE scanning</label>
        <label><input type="checkbox" id="radioCapture"> Save packet captures to SD</label>
      </div>
      <div class="field" style="margin-top:14px"><label for="radioWatch">Additional watchlist (one per line)</label>
        <textarea id="radioWatch" rows="3" maxlength="1024" placeholder="oui:B4:1E:52&#10;name:Penguin&#10;cid:034D&#10;svc:FC81"></textarea></div>
      <div class="field" style="margin-top:12px"><label for="radioTarget">Track a MAC address (RSSI tone; empty to stop)</label>
        <input id="radioTarget" type="text" maxlength="17" placeholder="AA:BB:CC:DD:EE:FF"></div>
      <div style="display:flex;gap:10px;flex-wrap:wrap;margin-top:14px">
        <button class="btn primary" id="radioSave">APPLY RADIO SETTINGS</button>
        <a class="btn" href="/api/radio/log" download>Radio events</a>
        <a class="btn" href="/api/radio/pcap" download>WiFi PCAP</a>
        <a class="btn" href="/api/radio/ble" download>BLE advertisements</a>
        <button class="btn ghost" id="radioFilesBtn">Saved sessions</button>
      </div>
      <div class="hint" id="radioCounts"></div><div class="hint" id="radioFiles"></div>
      <div style="overflow-x:auto"><table><thead><tr><th>Device / name</th><th>Evidence</th><th>RSSI</th><th>Age</th></tr></thead><tbody id="radioRows"></tbody></table></div>
      <div class="hint">OUI and company IDs are shared by unrelated devices. RSSI indicates received signal strength, not distance.
        An IR timing match plus a nearby radio candidate does not establish that they are the same device.</div>
      <div class="hint">Inspired by <a href="https://colonelpanic.tech/" target="_blank" rel="noopener">Colonel Panic</a> and
        <a href="https://github.com/colonelpanichacks/oui-spy-unified-blue" target="_blank" rel="noopener">OUI Spy Unified Blue</a>.</div>
    </div>
  </section>

  <!-- ============ WARDRIVE ============ -->
  <section id="wardrive" hidden>
    <div class="hint">Camera, OPT101 and radio detection keep running while wardriving. Set channel hopping in the <a href="#" id="wardriveScanner">Scanner tab</a>.</div>
    <div class="card">
      <div class="row-head"><strong>WiFi wardriver (WiGLE)</strong>
        <span style="flex:1"></span>
        <label class="switch"><input type="checkbox" id="wdEn"><span class="slider"></span></label></div>
      <div class="hint" style="margin-top:0">Logs nearby 2.4&nbsp;GHz WiFi APs to a
        <b>separate</b> WiGLE-format CSV (<code>/wardrive/wigle_*.csv</code>), GPS-tagged -
        upload straight to wigle.net. Runs alongside IR detection. Dashboard mode pauses
        full-channel surveys while a phone is connected; current-channel sniffing continues.
        In field mode, beacons are logged as the radio hops. Enable this toggle before driving.</div>

      <div class="grid">
        <div class="tile"><div class="k">APs (last scan)</div><div class="v" id="wdTotal">--</div></div>
        <div class="tile"><div class="k">New (last scan)</div><div class="v" id="wdNew">--</div></div>
        <div class="tile"><div class="k">Rows logged</div><div class="v" id="wdLogged">0</div></div>
        <div class="tile"><div class="k">Status</div><div class="v" id="wdStat">idle</div></div>
      </div>

      <div style="margin-top:16px"><a class="btn" href="/api/wardrive.csv" download>WiGLE CSV</a></div>
      <div class="hint" id="wdFixNote"></div>
    </div>
  </section>

  <!-- ============ SETTINGS ============ -->
  <section id="settings" hidden>
    <div class="card">
      <div class="row-head"><strong>Buzzer &amp; alert tones</strong>
        <span style="flex:1"></span>
        <label class="switch"><input type="checkbox" id="buzEn"><span class="slider"></span></label></div>
      <div class="hint" style="margin-top:0">Choose a sound for each device type or detection method. ALPR candidates use a retro blaster;
        Axon uses a siren; Ring and Meta use questioning tones. Use a
        <b>passive piezo buzzer</b> on <code>GPIO1 (D0)</code> to GND. Tones are stored on the device.</div>
      <div id="alertSounds"></div>
      <div class="hint">Custom tone library — select one of these slots in any sound assignment above.
        Test plays a preview, even when automatic alerts are muted.</div>
      <div id="tones"></div>

      <div class="presets" id="presets"></div>

      <div style="margin-top:16px;display:flex;gap:10px">
        <button class="btn primary" id="save">SAVE TO DEVICE</button>
        <button class="btn ghost" id="addTone">+ ADD TONE</button>
      </div>

      <div class="hint"><b>RTTTL format:</b> <code>name:d=4,o=6,b=180:notes</code> -
        <code>d</code> default duration, <code>o</code> octave, <code>b</code> BPM; notes like
        <code>8c6</code>, <code>16g#</code>, <code>p</code> (pause), <code>.</code> dotted.
        Hit <b>Test</b> to preview any tone through the buzzer.</div>
    </div>
  </section>
</div>

<footer>
  <span class="sig">- Your Pal <b>Kal</b></span>
  <span class="spacer"></span>
  <span>Flock Noir v0.5.1</span>
</footer>

<div class="toast" id="toast"></div>

<script>
const $=s=>document.querySelector(s), $$=s=>[...document.querySelectorAll(s)];
function fmt(x,d){return (x==null||isNaN(x))?'--':Number(x).toFixed(d);}
function toast(m){const t=$('#toast');t.textContent=m;t.classList.add('show');setTimeout(()=>t.classList.remove('show'),1600);}
const _wctx={};
function drawWaveOn(id,arr,color){
  const c=document.getElementById(id); if(!c)return;
  if(!_wctx[id])_wctx[id]=c.getContext('2d');
  const ctx=_wctx[id],W=c.width,H=c.height;
  ctx.clearRect(0,0,W,H);
  ctx.strokeStyle='rgba(95,118,129,.25)';ctx.lineWidth=1;
  ctx.beginPath();ctx.moveTo(0,H-1);ctx.lineTo(W,H-1);ctx.stroke();
  if(!arr||!arr.length)return;
  ctx.strokeStyle=color||'#3dfba0';ctx.lineWidth=2;ctx.beginPath();
  for(let i=0;i<arr.length;i++){
    const x=arr.length>1?i/(arr.length-1)*W:0;
    const y=H-4-(arr[i]/100)*(H-8);
    i?ctx.lineTo(x,y):ctx.moveTo(x,y);
  }
  ctx.stroke();
}

/* ---- tabs ---- */
const SECTIONS=['detector','scanner','camera','wardrive','settings'];
$$('.tab').forEach(b=>b.onclick=()=>{
  $$('.tab').forEach(x=>x.classList.toggle('on',x===b));
  SECTIONS.forEach(s=>$('#'+s).hidden = b.dataset.tab!==s);
  if(b.dataset.tab!=='camera') camStop();     // don't stream when not viewing
  if(b.dataset.tab==='camera') loadRecs();
  if(b.dataset.tab==='settings') loadSettings();
});

/* ---- recording ---- */
async function loadRecs(){
  try{const a=await (await fetch('/api/recs',{cache:'no-store'})).json();
    $('#recsList').innerHTML = a.length ? a.map(f=>
      '<div class="recfile"><span>'+f.name+'</span><span class="dim">'+(f.size/1024).toFixed(0)+
      ' KB</span><a class="btn sm" href="/api/rec/get?f='+encodeURIComponent(f.name)+'" download>get</a></div>'
      ).join('') : '<span class="dim">no recordings yet</span>';
  }catch(e){}
}
$('#recsRefresh').onclick=loadRecs;
$('#recBtn').onclick=async()=>{
  const recording=$('#recBtn').classList.contains('on');
  if(!recording){
    const wa=$('#recAudio').checked?'1':'0';
    const r=await (await fetch('/api/rec',{method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'action=start&audio='+wa})).json();
    if(r.ok){ if(!camOn)$('#camBtn').click(); }
    else toast('record failed - SD card in?');
  } else {
    await fetch('/api/rec',{method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'action=stop'});
    setTimeout(loadRecs,700);
  }
};

/* ---- live camera view ---- */
let camOn=false, camFrames=0, camT0=0;
const camImg=$('#cam');
function camStop(){camOn=false;$('#camBtn').innerHTML='START LIVE VIEW';
  $('#camState').textContent='OFFLINE';$('#camFps').textContent='';camImg.removeAttribute('src');}
function camNext(){if(camOn)camImg.src='/api/frame.jpg?t='+Date.now();}
camImg.onload=()=>{if(!camOn)return;camFrames++;const n=performance.now();
  if(n-camT0>=1000){$('#camFps').textContent=camFrames+' fps';camFrames=0;camT0=n;}
  setTimeout(camNext,250);};
camImg.onerror=()=>{if(camOn)setTimeout(camNext,400);};
$('#camBtn').onclick=()=>{camOn=!camOn;
  if(camOn){$('#camBtn').innerHTML='STOP';$('#camState').textContent='LIVE';
    camT0=performance.now();camFrames=0;camNext();}
  else camStop();};
$('#wdEn').onchange=()=>fetch('/api/wardrive',{method:'POST',
  headers:{'Content-Type':'application/x-www-form-urlencoded'},
  body:'en='+($('#wdEn').checked?'1':'0')});
const post=(u,en)=>fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'en='+(en?'1':'0')});
$('#mute').onchange=()=>post('/api/mute',$('#mute').checked);
$('#irEn').onchange=()=>post('/api/irsensor',$('#irEn').checked);

/* ---- detector polling ---- */
let blink=0;
async function tick(){
  try{
    const s=await (await fetch('/api/status',{cache:'no-store'})).json();
    $('#led').style.background=(blink^=1)?'var(--green)':'var(--green-d)';
    const b=$('#banner');
    if(s.irDet){b.className='banner alert';
      $('#bannerTxt').textContent=(s.radioNearby?'IR + RADIO NEARBY':'IR TIMING MATCH')+'  -  '+fmt(s.irFreq,1)+' Hz, '+fmt(s.irDuty*100,0)+'% duty';}
    else if(s.detected){b.className='banner alert';
      $('#bannerTxt').textContent='CAMERA PULSE PATTERN  -  '+fmt(s.freq,1)+' Hz, '+fmt(s.duty*100,0)+'% duty, conf '+fmt(s.confidence,2);}
    else if(s.alprAlert){b.className='banner alert';
      $('#bannerTxt').textContent=(s.alprAlert.assessment||'possible_camera').replaceAll('_',' ').toUpperCase()+' / '+s.alprAlert.category.toUpperCase()+' - '+s.alprAlert.method+' (tier '+s.alprAlert.tier+')';}
    else{b.className='banner clear';$('#bannerTxt').textContent='SCANNING FOR ALPR EVIDENCE...';}
    const f=s.fusion||{}, ages=f.ageMs||[];
    ['Ir','Camera','Ble','Wifi'].forEach((id,i)=>{
      $('#fusion'+id).textContent=ages[i]==null?'No recent match':fmt(ages[i]/1000,1)+'s ago';
      $('#fusion'+id).className=ages[i]==null?'dim':'ok';
    });
    $('#fusionSummary').textContent=(f.assessment||'no_recent_evidence').replaceAll('_',' ').toUpperCase()+
      (f.mask?' | '+f.method+' | 3s window':'');
    $('#readyIr').textContent=!s.irEn?'Off — enable after wiring':s.irClipped?'Clipping — reduce light':fmt(s.irSampleHz,0)+' samples/s — verify light response';
    $('#readyCamera').textContent=s.cameraReady?fmt(s.fps,1)+' analysis fps':'Camera unavailable';
    const focused=s.profile==='alpr';
    $('#profileState').textContent=focused?'ALPR focus active — automatic sounds for camera candidates':'General alerts active — all device types can sound';
    $('#generalProfileState').textContent=focused?'ALPR focus is active. Other devices still appear and log; select General alerts to hear their sounds.':'General alerts active. Optical and ALPR detection continue.';
    const ga=s.radioAlert;
    $('#generalBanner').className='banner '+(ga?'alert':'clear');
    $('#generalBannerTxt').textContent=ga?ga.category.toUpperCase()+' — '+ga.method+' (tier '+ga.tier+')':'SCANNING FOR DEVICE SIGNATURES...';
    $('#freq').innerHTML=fmt(s.freq,1)+'<small> Hz</small>';
    $('#duty').innerHTML=fmt(s.duty*100,0)+'<small> %</small>';
    $('#conf').style.width=Math.round((s.confidence||0)*100)+'%';
    // GPS health: chars prove the module is wired + talking
    if(!(s.gpsChars>0)) $('#fix').innerHTML='<span class="no">NO DATA</span>';
    else if(s.fix)      $('#fix').innerHTML='<span class="ok">FIX</span>';
    else                $('#fix').innerHTML='<span style="color:var(--amber)">acquiring</span>';
    $('#sats').innerHTML=(s.sats??'--')+' <small style="color:var(--mut)">hdop '+fmt(s.hdop,1)+'</small>';
    $('#pos').textContent=s.fix?(fmt(s.lat,6)+', '+fmt(s.lon,6)):(s.gpsChars>0?'searching...':'no gps');
    // live signal scope (camera)
    drawWaveOn('wave',s.wave||[],'#3dfba0');
    $('#sigMeta').textContent='amp '+(s.amp??0)+'  |  '+fmt(s.freq,1)+' Hz';
    // IR photodiode sensor
    if(document.activeElement!==$('#irEn'))$('#irEn').checked=!!s.irEn;
    drawWaveOn('irwave',s.irWave||[],s.irDet?'#ff4d5e':'#57b6ff');
    $('#irMeta').textContent=(s.irEn?('amp '+(s.irAmp??0)+'  |  '+fmt(s.irFreq,1)+' Hz'):'');
    $('#irStat').innerHTML = !s.irEn ? 'off'
      : (s.irDet ? '<span class="no">PULSE DETECTED</span>'
        : (s.irPresent ? '<span class="ok">sampling - verify response to light</span>'
          : '<span style="color:var(--amber)">armed - no sensor signal</span>'));
    $('#irHealth').textContent=s.irSampleHz==null?'':
      'Sampling '+fmt(s.irSampleHz,0)+' Hz | pulse '+fmt(s.irPulseMs,1)+' ms | raw '+s.irRaw+
      ' | baseline '+s.irBaseline+' | noise '+fmt(s.irNoise,1)+' | gaps '+s.irGaps+
      (s.irClipped?' | ADC CLIPPING - reduce incoming light':'');
    // system / QoL
    if(document.activeElement!==$('#mute'))$('#mute').checked=!!s.muted;
    const up=s.uptime||0, hh=Math.floor(up/3600), mm=Math.floor((up%3600)/60);
    $('#sysInfo').textContent='uptime '+hh+'h '+mm+'m  |  SD '+(s.sdFree??'?')+'/'+(s.sdTotal??'?')+' MB free';
    $('#fps').innerHTML=fmt(s.fps,0)+'<small> fps</small>';
    $('#sd').innerHTML=s.sd?'<span class="ok">ready</span>':'<span class="no">no card</span>';
    $('#count').textContent=(s.logged||0)+(s.radioEvents||0);
    // recording button/state
    if(s.rec){$('#recBtn').classList.add('on');
      $('#recBtn').innerHTML='STOP '+(s.recSecs||0)+'s';
      $('#recStat').textContent=(s.recAudio?'video+audio ':'video ')+(s.recFrames||0)+' frames';}
    else{$('#recBtn').classList.remove('on');$('#recBtn').innerHTML='REC';$('#recStat').textContent='';}
    // wardrive tiles
    $('#wdTotal').textContent=s.wdTotal??'--';
    $('#wdNew').textContent=s.wdNew??'--';
    $('#wdLogged').textContent=s.wdLogged??0;
    $('#wdStat').innerHTML=s.wd?(s.wdScan?'<span class="ok">scanning</span>':'<span class="ok">on</span>'):'<span class="dim">off</span>';
    if(document.activeElement!==$('#wdEn'))$('#wdEn').checked=!!s.wd;
    $('#wdFixNote').innerHTML=s.fix?'':'<b class="no">No GPS fix</b> - WiGLE logging waits for a current position fix.';
    if(s.time)$('#clock').textContent=s.time.replace('T',' ').replace('Z','');
    const rows=$('#rows');
    if(s.recent&&s.recent.length){rows.innerHTML=s.recent.map(a=>
      '<tr><td>'+a.t+'</td><td>'+escapeHtml((a.src||'')+(a.evidence?' / '+a.evidence:''))+'</td><td>'+fmt(a.lat,6)+'</td><td>'+fmt(a.lon,6)+
      '</td><td class="right ok">'+fmt(a.hz,1)+'</td><td class="right">'+fmt(a.duty*100,0)+
      '%</td><td class="right">'+fmt(a.conf,2)+'</td></tr>').join('');}
  }catch(e){$('#led').style.background='var(--red)';}
}
setInterval(tick,500); tick();

/* ---- radio controls; shared Pi UI hides unsupported hardware ---- */
function escapeHtml(value){return String(value??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));}
let radioLoaded=false,radioBusy=false,radioHardware='xiao';
async function pollRadio(){
  if(radioBusy)return;radioBusy=true;
  try{
    const response=await fetch('/api/radio',{cache:'no-store'});
    if(!response.ok)return;
    const r=await response.json();if(!r.supported)return;
    radioHardware=r.hardware||'xiao';
    if(r.modeHint)$('#radioModeHint').textContent=r.modeHint;
    $('#radioCard').hidden=false;
    $('#detectorRadioCard').hidden=false;
    if(!radioLoaded){
      $('#radioMode').value=r.mode;$('#radioChannel').value=r.channel;
      $('#radioHop').value=r.hop||'priority';
      $('#radioBle').checked=r.ble;$('#radioCapture').checked=r.capture;
      $('#radioWatch').value=r.watch;$('#radioTarget').value=r.target;radioLoaded=true;
    }
    $('#readyBle').textContent=r.ble && r.bleReady && r.bleScanning?'Scanning':r.ble?'Starting / unavailable':'Off';
    $('#readyWifi').textContent=r.wifiReady?(r.mode==='field'?'Hopping '+(r.hop==='all'?'1–11':'1/6/11'):'Channel '+r.channel+' — field mode broadens coverage'):'Unavailable';
    $('#radioHealth').textContent=r.mode+' | channel '+r.channel;
    $('#radioCounts').textContent='Packets '+r.packets+' | queue drops '+r.dropped+' | SD errors '+r.logErrors+
      (r.wifiReady?'':' | WiFi unavailable')+(r.ble && !r.bleReady?' | BLE unavailable':'')+
      (r.captureFull?' | Capture limit reached':'')+(r.detail?' | '+r.detail:'');
    $('#detectorRadioHealth').textContent='Flock-You scanning | '+r.mode+' | channel '+r.channel+
      ' | radio events '+(r.events||0)+(r.wifiReady?'':' | WiFi unavailable')+
      (r.ble && r.bleReady?' | BLE scanning':' | BLE off / unavailable');
    if(!$('#scanner').hidden || !$('#detector').hidden){
      const devices=await (await fetch('/api/radio/devices',{cache:'no-store'})).json();
      const sorted=devices.sort((a,b)=>(b.alpr&&b.tier>=2?1:0)-(a.alpr&&a.tier>=2?1:0)||b.tier-a.tier||a.ageMs-b.ageMs);
      const render=d=>
        '<tr><td>'+escapeHtml(d.mac)+'<br><span class="dim">'+escapeHtml(d.protocol+' '+(d.name||d.droneId||''))+
        '</span></td><td>'+escapeHtml(d.category||'Advertisement')+'<br><span class="dim">'+escapeHtml(d.method)+
        ' (tier '+escapeHtml(d.tier)+')</span></td><td>'+escapeHtml(d.rssi)+' dBm</td><td>'+fmt(d.ageMs/1000,1)+'s</td></tr>';
      const filter=$('#deviceFilter').value;
      $('#radioRows').innerHTML=sorted.filter(d=>d.tier>0 && (filter==='all' || (filter==='alpr'?d.alpr:(d.category||'').toLowerCase().includes(filter)))).slice(0,64).map(render).join('')||'<tr><td colspan="4" class="dim">No matching devices yet</td></tr>';
      $('#detectorRadioRows').innerHTML=sorted.filter(d=>d.alpr && d.tier>0).slice(0,8).map(render).join('')||
        '<tr><td colspan="4" class="dim">No matching radio devices yet</td></tr>';
    }
  }catch(e){}finally{radioBusy=false;}
}
async function setProfile(profile){
  try{
    const response=await fetch('/api/profile',{method:'POST',body:new URLSearchParams({profile})});
    if(!response.ok)throw new Error();
    radioLoaded=false;await pollRadio();await tick();toast(profile==='alpr'?'ALPR focus active':'General alerts active');
  }catch(e){toast('Could not save detection profile');}
}
$('#alprProfile').onclick=()=>setProfile('alpr');
$('#generalProfile').onclick=()=>setProfile('general');
$('#deviceFilter').onchange=()=>pollRadio();
$('#wardriveScanner').onclick=e=>{e.preventDefault();$('[data-tab=scanner]').click();};
$('#radioSave').onclick=async()=>{
  const body=new URLSearchParams({mode:$('#radioMode').value,channel:$('#radioChannel').value,
    hop:$('#radioHop').value,
    ble:$('#radioBle').checked?'1':'0',capture:$('#radioCapture').checked?'1':'0',
    watch:$('#radioWatch').value,target:$('#radioTarget').value.trim()});
  try{
    const response=await fetch('/api/radio',{method:'POST',body});
    if(!response.ok){toast('Invalid radio settings or storage failure');return;}
    toast($('#radioMode').value==='field'?(radioHardware==='pi'?'Monitor adapter hopping; dashboard stays on':'Field mode: hold BOOT to return'):'Radio settings saved');
  }catch(e){toast('Could not save radio settings');}
};
$('#radioFilesBtn').onclick=async()=>{
  try{
    const files=await (await fetch('/api/radio/files')).json();
    $('#radioFiles').innerHTML=files.length?files.map(f=>'<a href="/api/radio/file?name='+
      encodeURIComponent(f.name)+'" download>'+escapeHtml(f.name)+'</a> ('+f.size+' bytes)').join('<br>'):'No saved radio sessions';
  }catch(e){toast('Could not load sessions');}
};
setInterval(pollRadio,2500);pollRadio();

/* ---- settings ---- */
const PRESETS={
 'Power Rangers':'MMPR:d=16,o=7,b=400:c#8,p,c#8,p,b,c#8,p,e8,p,c#8',
 'Nokia':'Nokia:d=4,o=5,b=225:8e6,8d6,f#,g#,8c#6,8b,d,e,8b,8a,c#,e,2a',
 'Mario':'Mario:d=4,o=5,b=100:16e6,16e6,32p,8e6,16c6,8e6,8g6,8p,8g',
 'Alarm':'Alarm:d=8,o=6,b=180:c,p,c,p,c7,p,c7,p,g,p,g',
 'Chirp':'Chirp:d=32,o=7,b=200:c,p,c,p,c'
};
let MAXTONES=5, soundPresets=[], alertKinds=[];
function toneCard(i,name,rtttl){
 return '<div class="tone" data-i="'+i+'"><div class="thead"><span class="idx">#'+i+'</span>'+
   '<input type="text" maxlength="48" class="tname grow" value="'+escapeHtml(name||'')+'" placeholder="tone name">'+
   '<button class="btn sm test">Test</button><button class="btn sm ghost del">x</button></div>'+
   '<textarea class="trtttl" maxlength="1024" spellcheck="false">'+escapeHtml(rtttl||'')+'</textarea></div>';
}
function soundChoices(){
 const slots=$$('#tones .tone').map((t,i)=>({id:'slot:'+i,name:'#'+i+' '+t.querySelector('.tname').value}));
 return soundPresets.concat(slots);
}
function renderAssignments(){
 const options=soundChoices();
 $('#alertSounds').innerHTML=alertKinds.map(k=>'<div class="field"><label for="sound_'+k.id+'">'+escapeHtml(k.name)+
   '</label><div style="display:flex;gap:8px"><select style="flex:1;min-width:0" id="sound_'+k.id+'" data-kind="'+k.id+'">'+
   options.map(p=>'<option value="'+p.id+'" '+(p.id===k.sound?'selected':'')+'>'+escapeHtml(p.name)+'</option>').join('')+
   '</select><button class="btn sm sound-test" data-kind="'+k.id+'">Test</button></div></div>').join('');
 $$('#alertSounds select').forEach(s=>s.onchange=()=>{alertKinds.find(k=>k.id===s.dataset.kind).sound=s.value;});
 $$('.sound-test').forEach(b=>b.onclick=async()=>{
   const sound=$('#sound_'+b.dataset.kind).value;
   const args=new URLSearchParams();
   if(sound.startsWith('slot:'))args.set('rtttl',$$('#tones .trtttl')[Number(sound.slice(5))].value);
   else args.set('sound',sound);
   try{const r=await fetch('/api/test',{method:'POST',body:args});toast(r.ok?'Playing preview':'Preview failed');}
   catch(e){toast('Preview failed');}
 });
}
async function loadSettings(){
 try{
  const response=await fetch('/api/settings',{cache:'no-store'});
  if(!response.ok)throw new Error();
  const s=await response.json();MAXTONES=s.max||5;
  $('#buzEn').checked=!!s.enabled;soundPresets=s.soundPresets||[];alertKinds=s.alertKinds||[];
  $('#tones').innerHTML=s.tones.map((t,i)=>toneCard(i,t.name,t.rtttl)).join('');
  $('#presets').innerHTML='<span class="dim" style="align-self:center;font-size:11px">presets:</span>'+
    Object.keys(PRESETS).map(k=>'<button class="chip" data-p="'+k+'">'+k+'</button>').join('');
  bindSettings();renderAssignments();
 }catch(e){toast('Could not load settings');}
}
function bindSettings(){
 $$('#tones .test').forEach(b=>b.onclick=async e=>{
   const args=new URLSearchParams({rtttl:e.target.closest('.tone').querySelector('.trtttl').value});
   try{const r=await fetch('/api/test',{method:'POST',body:args});toast(r.ok?'Playing preview':'Preview failed');}
   catch(e){toast('Preview failed');}
 });
 $$('#tones .tname').forEach(n=>n.onchange=renderAssignments);
 $$('#tones .del').forEach(b=>b.onclick=e=>{
   if($$('#tones .tone').length<=1){toast('Keep at least one');return;}
   const card=e.target.closest('.tone'),removed=Number(card.dataset.i);
   alertKinds.forEach(k=>{if(k.sound.startsWith('slot:')){
     const old=Number(k.sound.slice(5));if(old===removed)k.sound='chirp';else if(old>removed)k.sound='slot:'+(old-1);
   }});
   card.remove();renumber();renderAssignments();
 });
 $$('#presets .chip').forEach(c=>c.onclick=()=>addTone(c.dataset.p,PRESETS[c.dataset.p]));
}
function renumber(){$$('#tones .tone').forEach((t,i)=>{t.dataset.i=i;t.querySelector('.idx').textContent='#'+i;});}
function addTone(name,rtttl){
 if($$('#tones .tone').length>=MAXTONES){toast('Max '+MAXTONES+' tones');return;}
 $('#tones').insertAdjacentHTML('beforeend',toneCard($$('#tones .tone').length,name,rtttl));
 bindSettings();renderAssignments();
}
$('#addTone').onclick=()=>addTone('Custom','Custom:d=8,o=6,b=180:c,e,g');
$('#save').onclick=async()=>{
 const tones=$$('#tones .tone'),p=new URLSearchParams();
 p.set('enabled',$('#buzEn').checked?'1':'0');p.set('alertIdx','0');p.set('count',tones.length);
 tones.forEach((t,i)=>{p.set('nm'+i,t.querySelector('.tname').value);p.set('rt'+i,t.querySelector('.trtttl').value);});
 alertKinds.forEach(k=>p.set('sound_'+k.id,k.sound));
 try{
  const response=await fetch('/api/settings',{method:'POST',body:p});
  if(!response.ok)throw new Error();
  toast('Saved to device');renumber();
 }catch(e){toast('Save failed — changes are not confirmed');}
};
</script></body></html>
)HTMLPAGE";
