#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <Arduino.h>

const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-TW">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>十秒挑戰雙競技 - 巨幕旗艦版</title>
  <link href="https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@700&family=Orbitron:wght@700;900&family=Share+Tech+Mono&family=Roboto+Mono:wght@700&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg: #050811;
      --cyan: #00f2fe;
      --green: #00ff87;
      --gold: #ffb703;
      --red: #ff0055;
      --purple: #a855f7;
      --text: #f0f4fc;
      --muted: #64748b;
      --panel-bg: #0b1324;
    }
    * { margin:0; padding:0; box-sizing:border-box; user-select:none; -webkit-tap-highlight-color: transparent; }
    html, body {
      width: 100%;
      height: 100%;
      overflow: hidden;
      background: radial-gradient(circle at 50% 30%, #0d172b 0%, #050811 100%);
      color: var(--text);
      font-family: system-ui, sans-serif;
    }

    #fxCanvas {
      position: fixed;
      inset: 0;
      width: 100vw;
      height: 100vh;
      pointer-events: none;
      z-index: 80;
    }

    .screen-shake {
      animation: stopShake 0.35s cubic-bezier(.36,.07,.19,.97) both;
    }
    @keyframes stopShake {
      15%, 85% { transform: translate3d(-4px, 0, 0); }
      30%, 70% { transform: translate3d(5px, 0, 0); }
      45%, 60% { transform: translate3d(-6px, 0, 0); }
    }

    .celebration-halo {
      position: fixed;
      inset: 0;
      pointer-events: none;
      z-index: 70;
      opacity: 0;
    }
    .halo-perfect {
      background: radial-gradient(circle, rgba(0,255,135,0.45) 0%, rgba(255,183,3,0.3) 45%, transparent 70%);
      opacity: 1;
      animation: haloFade 1s ease-out forwards;
    }
    .halo-godlike {
      background: radial-gradient(circle, rgba(0,242,254,0.45) 0%, rgba(0,255,135,0.2) 50%, transparent 70%);
      opacity: 1;
      animation: haloFade 1s ease-out forwards;
    }
    .halo-master {
      background: radial-gradient(circle, rgba(255,183,3,0.35) 0%, transparent 60%);
      opacity: 1;
      animation: haloFade 1s ease-out forwards;
    }
    .halo-miss {
      background: radial-gradient(circle, rgba(255,0,85,0.3) 0%, transparent 60%);
      opacity: 1;
      animation: haloFade 1s ease-out forwards;
    }
    @keyframes haloFade {
      0% { transform: scale(0.65); opacity: 0.95; }
      100% { transform: scale(1.4); opacity: 0; }
    }

    /* 頂部懸浮工具列 */
    .floating-toolbar {
      position: fixed;
      top: 10px;
      left: 10px;
      right: 10px;
      z-index: 90;
      display: flex;
      justify-content: space-between;
      align-items: center;
      transition: opacity 0.3s, transform 0.3s;
      pointer-events: none;
    }
    .toolbar-left, .toolbar-right {
      display: flex;
      gap: 6px;
      align-items: center;
      pointer-events: auto;
      flex-wrap: wrap;
    }
    .pill-btn {
      background: rgba(15, 23, 42, 0.82);
      backdrop-filter: blur(10px);
      -webkit-backdrop-filter: blur(10px);
      border: 1px solid rgba(255, 255, 255, 0.15);
      color: #e2e8f0;
      padding: 6px 12px;
      border-radius: 30px;
      font-size: 0.82rem;
      font-weight: 700;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      gap: 5px;
      text-decoration: none;
      box-shadow: 0 4px 15px rgba(0,0,0,0.4);
      transition: all 0.2s;
    }
    .pill-btn:hover {
      background: rgba(0, 242, 254, 0.2);
      border-color: var(--cyan);
      color: var(--cyan);
      transform: translateY(-1px);
    }
    .pill-btn:active { transform: scale(0.95); }

    /* 模式切換按鈕高亮 */
    .btn-mode {
      background: linear-gradient(135deg, rgba(168,85,247,0.35) 0%, rgba(0,242,254,0.35) 100%);
      border-color: var(--cyan);
      color: #fff;
    }
    .btn-fs {
      background: linear-gradient(135deg, rgba(255,183,3,0.35) 0%, rgba(251,133,0,0.35) 100%);
      border-color: var(--gold);
      color: var(--gold);
    }
    .btn-wifi {
      border-color: rgba(0, 242, 254, 0.4);
      color: var(--cyan);
    }
    .btn-wifi.online {
      border-color: var(--green);
      color: var(--green);
      background: rgba(0, 255, 135, 0.15);
    }

    /* 浮動隱藏/顯示控制紐 */
    .btn-collapse {
      position: fixed;
      bottom: 12px;
      right: 12px;
      z-index: 95;
      background: rgba(15, 23, 42, 0.65);
      backdrop-filter: blur(8px);
      border: 1px solid rgba(255,255,255,0.12);
      color: var(--muted);
      width: 38px;
      height: 38px;
      border-radius: 50%;
      display: flex;
      justify-content: center;
      align-items: center;
      cursor: pointer;
      font-size: 1.1rem;
      transition: all 0.2s;
    }

    /* 中央全螢幕極限舞台 */
    .giant-arena {
      width: 100vw;
      height: 100vh;
      display: flex;
      flex-direction: column;
      justify-content: center;
      align-items: center;
      padding: 16px;
      position: relative;
    }

    /* 目標提示標語 */
    .target-banner {
      font-family: 'Chakra Petch', sans-serif;
      font-size: clamp(0.95rem, 2.3vw, 1.6rem);
      letter-spacing: 2px;
      color: var(--muted);
      margin-bottom: -1vh;
      display: flex;
      align-items: center;
      gap: 8px;
      transition: opacity 0.3s;
    }
    .target-banner strong {
      color: var(--gold);
      font-family: 'Orbitron', monospace;
      font-size: 1.25em;
    }

    /* 👑 超極限巨幕數字 (全螢幕最大化，等寬不晃動，加大字距) */
    .giant-digits {
      font-size: min(25vw, 32vh);
      font-weight: 900;
      line-height: 1;
      color: var(--gold);
      text-shadow: 0 0 45px rgba(255, 183, 3, 0.6), 0 0 90px rgba(255, 183, 3, 0.25);
      cursor: pointer;
      display: inline-flex;
      justify-content: center;
      align-items: center;
      gap: 0.04em;
      font-variant-numeric: tabular-nums;
      font-feature-settings: "tnum";
      white-space: nowrap;
      transform: translateZ(0);
      will-change: contents;
    }

    .d-slot {
      display: inline-block;
      width: 0.70em;
      text-align: center;
      flex-shrink: 0;
      margin: 0 0.02em;
    }
    .d-dot {
      display: inline-block;
      width: 0.32em;
      text-align: center;
      flex-shrink: 0;
      margin: 0 0.04em;
    }

    /* 手速模式下的單一超巨型擊打次數 (字體極限狂放大) */
    .tapper-digits {
      font-size: min(40vw, 44vh);
      font-weight: 900;
      line-height: 0.95;
      color: var(--cyan);
      text-shadow: 0 0 55px rgba(0, 242, 254, 0.75), 0 0 110px rgba(0, 242, 254, 0.35);
      cursor: pointer;
      font-family: 'Orbitron', 'Impact', sans-serif;
      letter-spacing: -2px;
    }

    /* 4 款零震動字體樣式 */
    .font-sci-fi { font-family: 'Orbitron', 'Chakra Petch', 'Trebuchet MS', sans-serif; letter-spacing: 0.02em; }
    .font-impact { font-family: 'Impact', 'Arial Black', sans-serif; letter-spacing: 0.04em; }
    .font-7seg   { font-family: 'Share Tech Mono', 'Courier New', monospace; letter-spacing: 0.05em; }
    .font-modern { font-family: 'Roboto Mono', 'SF Pro Mono', 'Consolas', monospace; letter-spacing: 0.03em; }

    .giant-digits.idle, .tapper-digits.idle {
      color: #64748b;
      text-shadow: none;
    }
    .giant-digits.running {
      color: var(--cyan);
      text-shadow: 0 0 50px rgba(0, 242, 254, 0.7), 0 0 100px rgba(0, 242, 254, 0.3);
    }
    .tapper-digits.running {
      color: var(--green);
      text-shadow: 0 0 55px rgba(0, 255, 135, 0.8), 0 0 120px rgba(0, 255, 135, 0.4);
    }
    .giant-digits.perfect, .tapper-digits.perfect {
      color: var(--green);
      text-shadow: 0 0 65px rgba(0, 255, 135, 0.95), 0 0 130px rgba(0, 255, 135, 0.5);
    }

    /* 懸浮結算成績卡 */
    .result-overlay {
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 6px;
      margin-top: 1.5vh;
      min-height: 85px;
    }
    .delta-display {
      font-family: 'Orbitron', monospace;
      font-size: clamp(2rem, 5.5vw, 4rem);
      font-weight: 900;
      letter-spacing: 1px;
    }
    .delta-fast { color: var(--cyan); text-shadow: 0 0 20px rgba(0,242,254,0.6); }
    .delta-slow { color: var(--red); text-shadow: 0 0 20px rgba(255,0,85,0.6); }
    .delta-zero { color: var(--green); text-shadow: 0 0 35px rgba(0,255,135,0.9); }

    .rank-tag {
      font-size: clamp(1.1rem, 2.5vw, 1.8rem);
      font-weight: 900;
      padding: 6px 20px;
      border-radius: 30px;
      background: rgba(255, 255, 255, 0.08);
      border: 1px solid rgba(255, 255, 255, 0.2);
      transition: all 0.3s;
    }
    .rank-tag.perfect {
      background: rgba(0, 255, 135, 0.25);
      border-color: var(--green);
      color: var(--green);
      box-shadow: 0 0 25px rgba(0, 255, 135, 0.5);
    }

    /* 底部狀態提示浮條 */
    .bottom-hint {
      position: fixed;
      bottom: 24px;
      left: 50%;
      transform: translateX(-50%);
      font-size: clamp(0.9rem, 2vw, 1.25rem);
      font-weight: 700;
      color: var(--cyan);
      background: rgba(15, 23, 42, 0.85);
      backdrop-filter: blur(10px);
      border: 1px solid rgba(0, 242, 254, 0.3);
      padding: 8px 24px;
      border-radius: 30px;
      display: flex;
      align-items: center;
      gap: 10px;
      white-space: nowrap;
      box-shadow: 0 4px 20px rgba(0,0,0,0.5);
      transition: all 0.25s;
    }
    .bottom-hint.locked {
      border-color: var(--gold);
      color: var(--gold);
      background: rgba(45, 28, 5, 0.9);
      box-shadow: 0 0 25px rgba(255, 183, 3, 0.6);
      transform: translateX(-50%) scale(1.05);
    }
    .bottom-hint.wait-reset {
      border-color: var(--purple);
      color: #e9d5ff;
      background: rgba(59, 7, 100, 0.9);
      box-shadow: 0 0 25px rgba(168, 85, 247, 0.6);
      transform: translateX(-50%) scale(1.03);
    }

    /* 抽屜通用 (Drawer) */
    .drawer-overlay {
      position: fixed;
      inset: 0;
      background: rgba(0,0,0,0.7);
      backdrop-filter: blur(5px);
      z-index: 150;
      opacity: 0;
      pointer-events: none;
      transition: opacity 0.3s;
    }
    .drawer-overlay.open { opacity: 1; pointer-events: auto; }
    .drawer-content {
      position: fixed;
      top: 0;
      right: 0;
      bottom: 0;
      width: 100%;
      max-width: 400px;
      background: var(--panel-bg);
      border-left: 1px solid rgba(0,242,254,0.3);
      box-shadow: -10px 0 30px rgba(0,0,0,0.8);
      z-index: 160;
      transform: translateX(100%);
      transition: transform 0.3s cubic-bezier(0.16, 1, 0.3, 1);
      display: flex;
      flex-direction: column;
      padding: 20px;
      gap: 16px;
      overflow-y: auto;
    }
    .drawer-overlay.open .drawer-content { transform: translateX(0); }
    .drawer-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      border-bottom: 1px solid rgba(255,255,255,0.1);
      padding-bottom: 12px;
    }
    .drawer-title { font-size: 1.15rem; font-weight: 800; color: var(--gold); }

    .form-group { display: flex; flex-direction: column; gap: 6px; }
    .form-label { font-size: 0.82rem; font-weight: 700; color: var(--muted); }
    .form-input, .form-select {
      background: rgba(0,0,0,0.5);
      border: 1px solid rgba(255,255,255,0.15);
      color: #fff;
      padding: 10px 12px;
      border-radius: 8px;
      font-size: 0.9rem;
      outline: none;
    }
    .form-input:focus, .form-select:focus { border-color: var(--cyan); }
    .box-info {
      background: rgba(0, 242, 254, 0.08);
      border: 1px solid rgba(0, 242, 254, 0.25);
      border-radius: 10px;
      padding: 12px;
      font-size: 0.82rem;
      line-height: 1.5;
    }
    .record-item {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 10px 14px;
      border-radius: 10px;
      background: rgba(255,255,255,0.04);
      border: 1px solid rgba(255,255,255,0.06);
    }

    .hide-ui .floating-toolbar,
    .hide-ui .target-banner {
      opacity: 0;
      transform: translateY(-20px);
      pointer-events: none;
    }
  </style>
</head>
<body>

  <canvas id="fxCanvas"></canvas>
  <div class="celebration-halo" id="haloLayer"></div>

  <!-- 頂部懸浮工具列 -->
  <header class="floating-toolbar" id="toolbar">
    <div class="toolbar-left">
      <button class="pill-btn btn-mode" id="btnMode" onclick="toggleGameMode()">
        🎮 模式: ⏱️ 十秒節拍挑戰
      </button>
      <button class="pill-btn btn-fs" id="btnFullscreen" onclick="toggleFullscreen()">
        ⛶ 全螢幕
      </button>
      <button class="pill-btn btn-font" id="btnFont" onclick="cycleFont()" title="切換字體樣式">
        🔤 字體: 賽博科技
      </button>
    </div>
    <div class="toolbar-right">
      <button class="pill-btn btn-wifi" id="btnWifi" onclick="openWifiDrawer()">
        🌐 網路與雲端
      </button>
      <button class="pill-btn" id="btnSound" onclick="toggleSound()">🔊 音效開</button>
      <button class="pill-btn" onclick="openRecordsDrawer()">🏆 英雄榜</button>
    </div>
  </header>

  <div class="btn-collapse" id="btnCollapse" onclick="toggleUI()" title="切換精簡全螢幕">
    👁️
  </div>

  <!-- 中央巨幕舞台 -->
  <main class="giant-arena" id="arenaMain" onclick="handleTapScreen()">
    <!-- 標語 (依模式動態變化) -->
    <div class="target-banner" id="targetBanner">
      🎯 目標時間 <strong>10.000 秒</strong>
    </div>

    <!-- 視圖 1：十秒停秒挑戰機 (Time Lord) -->
    <div class="giant-digits font-sci-fi idle" id="boxTimeLord">
      <span class="d-slot" id="d0">0</span><span class="d-slot" id="d1">0</span><span class="d-dot">.</span><span class="d-slot" id="d2">0</span><span class="d-slot" id="d3">0</span><span class="d-slot" id="d4">0</span>
    </div>

    <!-- 視圖 2：十秒手速狂拍挑戰 (Speed Tapper) -->
    <div class="tapper-digits font-sci-fi idle" id="boxSpeedTapper" style="display:none;">
      0
    </div>

    <!-- 定格結算資訊 -->
    <div class="result-overlay">
      <div class="delta-display" id="deltaDisplay">--</div>
      <div class="rank-tag" id="rankTag">準備就緒</div>
    </div>
  </main>

  <!-- 底部即時狀態提示浮條 -->
  <div class="bottom-hint" id="bottomHint">
    <span>🔘 拍擊實體大按鈕（或點擊螢幕）開始！</span>
  </div>

  <!-- 浮動抽屜 1：英雄榜 -->
  <div class="drawer-overlay" id="recordsDrawer" onclick="closeAllDrawers(event)">
    <div class="drawer-content" onclick="event.stopPropagation()">
      <div class="drawer-header">
        <div class="drawer-title" id="drawerTitle">🏆 今日神手榜 (Top 5)</div>
        <button class="pill-btn" onclick="closeAllDrawers()">✕ 關閉</button>
      </div>
      <div id="drawerRecords" style="display:flex; flex-direction:column; gap:8px;">
        <div style="text-align:center; color:var(--muted); padding:20px;">尚無紀錄</div>
      </div>
      <button class="pill-btn" style="margin-top:auto; justify-content:center;" onclick="clearLeaderboard()">
        🗑️ 清除所有紀錄
      </button>
    </div>
  </div>

  <!-- 浮動抽屜 2：Wi-Fi 連線與雲端 OTA 更新 -->
  <div class="drawer-overlay" id="wifiDrawer" onclick="closeAllDrawers(event)">
    <div class="drawer-content" onclick="event.stopPropagation()">
      <div class="drawer-header">
        <div class="drawer-title">🌐 外部 Wi-Fi 與雲端更新</div>
        <button class="pill-btn" onclick="closeAllDrawers()">✕ 關閉</button>
      </div>

      <div class="box-info" id="wifiStatusBox">正在取得連線狀態...</div>

      <div style="font-weight:800; font-size:0.95rem; color:var(--cyan); margin-top:8px;">
        📡 連接教室 / 家用 Wi-Fi
      </div>
      <div class="form-group">
        <div style="display:flex; justify-content:space-between; align-items:center;">
          <span class="form-label">Wi-Fi 熱點名稱 (SSID)</span>
          <button class="pill-btn" style="padding:3px 8px; font-size:0.75rem;" onclick="scanWifi()">🔍 掃描</button>
        </div>
        <input type="text" class="form-input" id="wifiSsid" placeholder="輸入或下方選擇 Wi-Fi">
        <select class="form-select" id="wifiSelect" style="display:none;" onchange="selectSsid(this)">
          <option value="">-- 選擇掃描到的 Wi-Fi --</option>
        </select>
      </div>
      <div class="form-group">
        <span class="form-label">Wi-Fi 密碼</span>
        <input type="password" class="form-input" id="wifiPass" placeholder="若無密碼可留空">
      </div>
      <button class="pill-btn" style="justify-content:center; background:linear-gradient(135deg,#00f2fe,#4facfe); color:#000;" onclick="connectWifi()">
        🔗 儲存並連線 Wi-Fi
      </button>

      <div style="font-weight:800; font-size:0.95rem; color:var(--gold); margin-top:16px;">
        ☁️ 雲端線上下載韌體更新 (OTA)
      </div>
      <div class="box-info" style="border-color:rgba(255,183,3,0.3); background:rgba(255,183,3,0.06);">
        連上外部網路後，板子可直接從遠端載點下載最新韌體自動刷新：
      </div>
      <div class="form-group">
        <span class="form-label">韌體下載網址 (.bin URL)</span>
        <div style="display:flex; gap:6px; margin-bottom:4px;">
          <button class="pill-btn" style="padding:3px 8px; font-size:0.75rem;" onclick="setCloudUrl('drive')">📁 Google Drive 載點</button>
          <button class="pill-btn" style="padding:3px 8px; font-size:0.75rem;" onclick="setCloudUrl('github')">🐙 GitHub 載點</button>
        </div>
        <input type="text" class="form-input" id="cloudOtaUrl" value="https://drive.google.com/uc?export=download&id=1QwJxZ1gZpncdnnJL5c4IYgl0H-iliAxm">
      </div>
      <button class="pill-btn" style="justify-content:center; background:linear-gradient(135deg,#ffb703,#fb8500); color:#000;" onclick="startCloudOTA()">
        ☁️ 從雲端一鍵下載升級
      </button>

      <a href="/update" class="pill-btn" style="margin-top:auto; justify-content:center;">
        🚀 本地手動上傳韌體 (/update)
      </a>
    </div>
  </div>

  <script>
    const STATE_READY = 0;
    const STATE_RUNNING = 1;
    const STATE_RESULT = 2;

    const MODE_TIME_LORD = 0;
    const MODE_SPEED_TAPPER = 1;
    let currentGameMode = MODE_TIME_LORD;

    let currentState = STATE_READY;
    let localStartTime = 0;
    let animFrameId = null;
    let soundEnabled = true;
    let uiVisible = true;
    let currentHits = 0;

    const FONTS = [
      { id: 'font-sci-fi', name: '賽博科技' },
      { id: 'font-impact', name: '競技粗體' },
      { id: 'font-7seg',   name: '電子晶片' },
      { id: 'font-modern', name: '極簡等寬' }
    ];
    let fontIndex = parseInt(localStorage.getItem('timer10s_font') || '0', 10);

    const btnMode = document.getElementById('btnMode');
    const targetBanner = document.getElementById('targetBanner');
    const boxTimeLord = document.getElementById('boxTimeLord');
    const boxSpeedTapper = document.getElementById('boxSpeedTapper');

    const d0 = document.getElementById('d0');
    const d1 = document.getElementById('d1');
    const d2 = document.getElementById('d2');
    const d3 = document.getElementById('d3');
    const d4 = document.getElementById('d4');

    const deltaDisplay = document.getElementById('deltaDisplay');
    const rankTag = document.getElementById('rankTag');
    const bottomHint = document.getElementById('bottomHint');
    const btnFullscreen = document.getElementById('btnFullscreen');
    const btnFont = document.getElementById('btnFont');
    const btnWifi = document.getElementById('btnWifi');
    const haloLayer = document.getElementById('haloLayer');
    const recordsDrawer = document.getElementById('recordsDrawer');
    const wifiDrawer = document.getElementById('wifiDrawer');
    const drawerRecords = document.getElementById('drawerRecords');
    const drawerTitle = document.getElementById('drawerTitle');
    const fxCanvas = document.getElementById('fxCanvas');
    const fxCtx = fxCanvas.getContext('2d');

    function resizeCanvas() {
      fxCanvas.width = window.innerWidth;
      fxCanvas.height = window.innerHeight;
    }
    window.addEventListener('resize', resizeCanvas);
    resizeCanvas();

    function applyFont() {
      FONTS.forEach(f => {
        boxTimeLord.classList.remove(f.id);
        boxSpeedTapper.classList.remove(f.id);
      });
      boxTimeLord.classList.add(FONTS[fontIndex].id);
      boxSpeedTapper.classList.add(FONTS[fontIndex].id);
      btnFont.innerText = '🔤 字體: ' + FONTS[fontIndex].name;
      localStorage.setItem('timer10s_font', fontIndex.toString());
    }
    function cycleFont() {
      fontIndex = (fontIndex + 1) % FONTS.length;
      applyFont();
    }

    // 切換遊戲模式
    function toggleGameMode() {
      const newMode = (currentGameMode === MODE_TIME_LORD) ? MODE_SPEED_TAPPER : MODE_TIME_LORD;
      fetch('/api/mode?set=' + newMode, { method: 'POST' })
        .then(() => switchUIMode(newMode))
        .catch(() => switchUIMode(newMode));
    }

    function switchUIMode(mode) {
      currentGameMode = mode;
      currentState = STATE_READY;
      if (animFrameId) cancelAnimationFrame(animFrameId);

      if (mode === MODE_TIME_LORD) {
        btnMode.innerHTML = '🎮 模式: ⏱️ 十秒節拍挑戰';
        targetBanner.innerHTML = '🎯 目標時間 <strong>10.000 秒</strong>';
        boxTimeLord.style.display = 'inline-flex';
        boxSpeedTapper.style.display = 'none';
        updateDigitSlots(0);
        boxTimeLord.className = 'giant-digits ' + FONTS[fontIndex].id + ' idle';
      } else {
        btnMode.innerHTML = '🎮 模式: ⚡ 十秒手速挑戰';
        targetBanner.innerHTML = '⚡ 倒數 <strong>10.0 秒</strong> 瘋狂狂拍！';
        boxTimeLord.style.display = 'none';
        boxSpeedTapper.style.display = 'block';
        boxSpeedTapper.innerText = '0';
        boxSpeedTapper.className = 'tapper-digits ' + FONTS[fontIndex].id + ' idle';
      }

      deltaDisplay.innerText = '--';
      deltaDisplay.className = 'delta-display';
      rankTag.innerText = '準備就緒';
      rankTag.className = 'rank-tag';
      bottomHint.className = 'bottom-hint';
      bottomHint.innerHTML = '<span>🔘 拍擊實體大按鈕開始！</span>';
      drawerTitle.innerText = mode === MODE_TIME_LORD ? '🏆 今日神手榜 (Top 5)' : '⚡ 今日手速榜 (Top 5)';
      playTone(700, 'sine', 0.08, 0.25);
    }

    function updateDigitSlots(ms) {
      const s = Math.floor(ms / 1000);
      const m = Math.floor(ms % 1000);
      const sStr = (s < 10 ? '0' : '') + s;
      const mStr = (m < 100 ? (m < 10 ? '00' : '0') : '') + m;

      d0.innerText = sStr[0];
      d1.innerText = sStr[1];
      d2.innerText = mStr[0];
      d3.innerText = mStr[1];
      d4.innerText = mStr[2];
    }

    // Web Audio 音效產生器 (純淨無讀秒音，保留按鍵短音與勝利 fanfare)
    const audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    function playTone(freq, type = 'sine', duration = 0.08, vol = 0.3) {
      if (!soundEnabled || audioCtx.state === 'suspended') return;
      try {
        const osc = audioCtx.createOscillator();
        const gain = audioCtx.createGain();
        osc.type = type;
        osc.frequency.setValueAtTime(freq, audioCtx.currentTime);
        gain.gain.setValueAtTime(vol, audioCtx.currentTime);
        gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + duration);
        osc.connect(gain);
        gain.connect(audioCtx.destination);
        osc.start();
        osc.stop(audioCtx.currentTime + duration);
      } catch(e){}
    }

    function playFanfare() {
      if (!soundEnabled) return;
      [523, 659, 784, 1046, 1318].forEach((f, idx) => {
        setTimeout(() => playTone(f, 'triangle', 0.28, 0.45), idx * 90);
      });
    }

    // 1 秒慶祝粒子
    let particles = [];
    let particleAnimId = null;

    function triggerCelebrationFX(tier) {
      particles = [];
      const colors = tier === 'perfect' 
        ? ['#ffb703', '#00ff87', '#00f2fe', '#ffffff', '#ffd166']
        : tier === 'godlike' 
        ? ['#00ff87', '#00f2fe', '#ffffff']
        : ['#ffb703', '#ffd166'];

      const count = tier === 'perfect' ? 80 : tier === 'godlike' ? 45 : 20;
      const centerX = window.innerWidth / 2;
      const centerY = window.innerHeight / 2;

      for (let i = 0; i < count; i++) {
        const angle = Math.random() * Math.PI * 2;
        const speed = Math.random() * 12 + 4;
        particles.push({
          x: centerX,
          y: centerY,
          vx: Math.cos(angle) * speed,
          vy: Math.sin(angle) * speed - 2,
          size: Math.random() * 8 + 4,
          color: colors[Math.floor(Math.random() * colors.length)],
          rotation: Math.random() * 360,
          rotSpeed: (Math.random() - 0.5) * 15,
          alpha: 1
        });
      }

      if (particleAnimId) cancelAnimationFrame(particleAnimId);
      const fxStart = performance.now();

      function updateFX() {
        const elapsed = performance.now() - fxStart;
        fxCtx.clearRect(0, 0, fxCanvas.width, fxCanvas.height);

        if (elapsed > 1000) {
          fxCtx.clearRect(0, 0, fxCanvas.width, fxCanvas.height);
          particles = [];
          return;
        }

        const fade = 1 - (elapsed / 1000);
        particles.forEach(p => {
          p.x += p.vx;
          p.y += p.vy;
          p.vy += 0.35;
          p.rotation += p.rotSpeed;

          fxCtx.save();
          fxCtx.translate(p.x, p.y);
          fxCtx.rotate((p.rotation * Math.PI) / 180);
          fxCtx.fillStyle = p.color;
          fxCtx.globalAlpha = fade;
          fxCtx.fillRect(-p.size / 2, -p.size / 2, p.size, p.size);
          fxCtx.restore();
        });

        particleAnimId = requestAnimationFrame(updateFX);
      }
      particleAnimId = requestAnimationFrame(updateFX);
    }

    function toggleFullscreen() {
      if (!document.fullscreenElement) {
        document.documentElement.requestFullscreen().then(() => {
          btnFullscreen.innerHTML = '✖ 退出全螢幕';
        }).catch(() => {});
      } else {
        if (document.exitFullscreen) {
          document.exitFullscreen().then(() => {
            btnFullscreen.innerHTML = '⛶ 全螢幕';
          }).catch(() => {});
        }
      }
    }
    document.addEventListener('fullscreenchange', () => {
      btnFullscreen.innerHTML = document.fullscreenElement ? '✖ 退出全螢幕' : '⛶ 全螢幕';
    });

    function toggleUI() {
      uiVisible = !uiVisible;
      document.body.classList.toggle('hide-ui', !uiVisible);
      document.getElementById('btnCollapse').innerText = uiVisible ? '👁️' : '🕶️';
    }

    function toggleSound() {
      if (audioCtx.state === 'suspended') audioCtx.resume();
      soundEnabled = !soundEnabled;
      document.getElementById('btnSound').innerText = soundEnabled ? '🔊 音效開' : '🔇 靜音';
    }

    function handleTapScreen() {
      if (audioCtx.state === 'suspended') audioCtx.resume();
      fetch('/api/button', { method: 'POST' }).catch(()=>{});
    }

    // 本地平滑渲染
    function runLocalClock() {
      if (animFrameId) cancelAnimationFrame(animFrameId);
      function frame() {
        if (currentState === STATE_RUNNING) {
          const elapsed = performance.now() - localStartTime;
          if (currentGameMode === MODE_TIME_LORD) {
            updateDigitSlots(elapsed);
          } else {
            // 手速模式：倒數計時提示
            const remainMs = Math.max(0, 10000 - elapsed);
            const remainSec = (remainMs / 1000).toFixed(1);
            targetBanner.innerHTML = `⚡ 剩餘時間 <strong>${remainSec}s</strong> 瘋狂狂拍！`;
          }
          animFrameId = requestAnimationFrame(frame);
        }
      }
      animFrameId = requestAnimationFrame(frame);
    }

    // 狀態機處理：由後端事件驅動
    function applyState(data) {
      if (audioCtx.state === 'suspended') audioCtx.resume();

      if (data.mode !== undefined && data.mode !== currentGameMode) {
        switchUIMode(data.mode);
      }

      if (data.type === 'START') {
        currentState = STATE_RUNNING;
        localStartTime = performance.now();

        if (currentGameMode === MODE_TIME_LORD) {
          boxTimeLord.className = 'giant-digits ' + FONTS[fontIndex].id + ' running';
          bottomHint.className = 'bottom-hint';
          bottomHint.innerHTML = '<span>⚡ 計時狂飆中！第 2 下拍擊定格！</span>';
        } else {
          currentHits = data.hits || 1;
          boxSpeedTapper.innerText = currentHits;
          boxSpeedTapper.className = 'tapper-digits ' + FONTS[fontIndex].id + ' running';
          bottomHint.className = 'bottom-hint';
          bottomHint.innerHTML = '<span>⚡ 倒數 10 秒！火速狂拍按鍵！！！</span>';
        }

        deltaDisplay.innerText = '--';
        deltaDisplay.className = 'delta-display';
        rankTag.innerText = '挑戰進行中...';
        rankTag.className = 'rank-tag';
        playTone(950, 'sine', 0.1, 0.4);
        runLocalClock();
      }
      else if (data.type === 'HIT') {
        // 手速模式即時點擊加算
        currentHits = (data.h !== undefined) ? data.h : data.hits;
        boxSpeedTapper.innerText = currentHits;
        playTone(1000 + Math.min(currentHits * 15, 800), 'sine', 0.03, 0.2);
      }
      else if (data.type === 'STOP') {
        currentState = STATE_RESULT;
        if (animFrameId) cancelAnimationFrame(animFrameId);

        haloLayer.className = 'celebration-halo';
        void haloLayer.offsetWidth;

        if (currentGameMode === MODE_TIME_LORD) {
          // 十秒停秒結算
          const elapsed = data.elapsed;
          const delta = data.delta;
          const rank = data.rank;
          const absDelta = Math.abs(delta);
          updateDigitSlots(elapsed);

          if (delta === 0) {
            deltaDisplay.innerText = '±0.000s 完美命中！';
            deltaDisplay.className = 'delta-display delta-zero';
            boxTimeLord.className = 'giant-digits ' + FONTS[fontIndex].id + ' perfect';
            rankTag.className = 'rank-tag perfect';
            haloLayer.classList.add('halo-perfect');
            document.body.classList.add('screen-shake');
            setTimeout(() => document.body.classList.remove('screen-shake'), 350);
            triggerCelebrationFX('perfect');
            playFanfare();
            bottomHint.className = 'bottom-hint locked';
            bottomHint.innerHTML = '<span>👑 神之領域！全場最高榮耀！(1s)</span>';
          } else if (absDelta <= 50) {
            const deltaSec = (absDelta / 1000).toFixed(3);
            const sign = delta < 0 ? '-' : '+';
            deltaDisplay.innerText = `${sign}${deltaSec}s (${delta < 0 ? '快了' : '慢了'})`;
            deltaDisplay.className = 'delta-display delta-zero';
            boxTimeLord.className = 'giant-digits ' + FONTS[fontIndex].id;
            rankTag.className = 'rank-tag';
            haloLayer.classList.add('halo-godlike');
            triggerCelebrationFX('godlike');
            playTone(587, 'triangle', 0.25, 0.4);
            bottomHint.className = 'bottom-hint locked';
            bottomHint.innerHTML = '<span>🥇 人體節拍器！極限準度！(1s)</span>';
          } else if (absDelta <= 150) {
            const deltaSec = (absDelta / 1000).toFixed(3);
            const sign = delta < 0 ? '-' : '+';
            deltaDisplay.innerText = `${sign}${deltaSec}s (${delta < 0 ? '快了' : '慢了'})`;
            deltaDisplay.className = delta < 0 ? 'delta-display delta-fast' : 'delta-display delta-slow';
            boxTimeLord.className = 'giant-digits ' + FONTS[fontIndex].id;
            rankTag.className = 'rank-tag';
            haloLayer.classList.add('halo-master');
            triggerCelebrationFX('master');
            playTone(440, 'triangle', 0.2, 0.35);
            bottomHint.className = 'bottom-hint locked';
            bottomHint.innerHTML = '<span>🥈 時間管理大師！手感絕佳！(1s)</span>';
          } else {
            const deltaSec = (absDelta / 1000).toFixed(3);
            const sign = delta < 0 ? '-' : '+';
            deltaDisplay.innerText = `${sign}${deltaSec}s (${delta < 0 ? '快了' : '慢了'})`;
            deltaDisplay.className = delta < 0 ? 'delta-display delta-fast' : 'delta-display delta-slow';
            boxTimeLord.className = 'giant-digits ' + FONTS[fontIndex].id;
            rankTag.className = 'rank-tag';
            if (absDelta > 500) haloLayer.classList.add('halo-miss');
            playTone(300, 'sawtooth', 0.18, 0.35);
            bottomHint.className = 'bottom-hint locked';
            bottomHint.innerHTML = '<span>🔒 定格結算防連點中... (1s)</span>';
          }
          rankTag.innerText = rank;
          addRecord('timelord', elapsed, delta, rank);
        } else {
          // 手速挑戰結算
          const hits = data.hits;
          const cps = (data.cps || (hits / 10.0)).toFixed(1);
          const rank = data.rank;
          boxSpeedTapper.innerText = hits;
          targetBanner.innerHTML = '⚡ 時間到！<strong>挑戰完成</strong>';

          deltaDisplay.innerText = `${hits} 下 (平均 ${cps} 次/秒)`;
          deltaDisplay.className = 'delta-display delta-zero';
          rankTag.innerText = rank;

          if (hits >= 75) {
            boxSpeedTapper.className = 'tapper-digits ' + FONTS[fontIndex].id + ' perfect';
            rankTag.className = 'rank-tag perfect';
            haloLayer.classList.add('halo-perfect');
            document.body.classList.add('screen-shake');
            setTimeout(() => document.body.classList.remove('screen-shake'), 350);
            triggerCelebrationFX('perfect');
            playFanfare();
          } else {
            boxSpeedTapper.className = 'tapper-digits ' + FONTS[fontIndex].id;
            haloLayer.classList.add('halo-godlike');
            triggerCelebrationFX('godlike');
            playTone(660, 'triangle', 0.25, 0.4);
          }

          bottomHint.className = 'bottom-hint locked';
          bottomHint.innerHTML = '<span>⚡ 結算中！(1s)</span>';
          addRecord('tapper', hits, cps, rank);
        }
      }
      else if (data.type === 'WAIT_RESET') {
        bottomHint.className = 'bottom-hint wait-reset';
        bottomHint.innerHTML = '<span>🔄 拍擊按鈕 歸零準備</span>';
      }
      else if (data.type === 'RESET') {
        currentState = STATE_READY;
        if (animFrameId) cancelAnimationFrame(animFrameId);

        if (currentGameMode === MODE_TIME_LORD) {
          updateDigitSlots(0);
          boxTimeLord.className = 'giant-digits ' + FONTS[fontIndex].id + ' idle';
          targetBanner.innerHTML = '🎯 目標時間 <strong>10.000 秒</strong>';
        } else {
          boxSpeedTapper.innerText = '0';
          boxSpeedTapper.className = 'tapper-digits ' + FONTS[fontIndex].id + ' idle';
          targetBanner.innerHTML = '⚡ 倒數 <strong>10.0 秒</strong> 瘋狂狂拍！';
        }

        deltaDisplay.innerText = '--';
        deltaDisplay.className = 'delta-display';
        rankTag.innerText = '準備就緒';
        rankTag.className = 'rank-tag';
        bottomHint.className = 'bottom-hint';
        bottomHint.innerHTML = '<span>🔘 拍擊按鈕開始！</span>';
        playTone(800, 'sine', 0.08, 0.25);
      }
    }

    // 抽屜控制
    function openRecordsDrawer() {
      closeAllDrawers();
      renderDrawer();
      recordsDrawer.classList.add('open');
    }
    function openWifiDrawer() {
      closeAllDrawers();
      checkWifiStatus();
      wifiDrawer.classList.add('open');
    }
    function closeAllDrawers() {
      recordsDrawer.classList.remove('open');
      wifiDrawer.classList.remove('open');
    }

    // Wi-Fi 狀態與連線邏輯
    function checkWifiStatus() {
      fetch('/api/wifi/status')
        .then(r => r.json())
        .then(d => {
          const box = document.getElementById('wifiStatusBox');
          if (d.connected) {
            btnWifi.classList.add('online');
            btnWifi.innerHTML = '🌐 Wi-Fi: ' + d.ssid;
            box.innerHTML = `
              <div style="color:var(--green); font-weight:800;">✅ 已連線外部 Wi-Fi: <strong>${d.ssid}</strong></div>
              <div>區網 IP: <strong>http://${d.ip}/</strong></div>
              <div>mDNS 網址: <strong>http://${d.mdns}/</strong></div>
              <div style="font-size:0.75rem; color:var(--muted); margin-top:4px;">同 Wi-Fi 內的手機或電腦輸入上述網址皆可直連！</div>
            `;
          } else {
            btnWifi.classList.remove('online');
            btnWifi.innerHTML = '🌐 網路與雲端';
            box.innerHTML = `
              <div style="color:var(--gold); font-weight:700;">⚠️ 尚未連接外部 Wi-Fi</div>
              <div>本機熱點保底: <strong>http://${d.ap_ip}/</strong></div>
              <div style="font-size:0.75rem; color:var(--muted); margin-top:4px;">請在下方設定連上教室/手機熱點，即可讓其他電腦存取或進行雲端更新。</div>
            `;
          }
        })
        .catch(() => {});
    }

    function scanWifi() {
      const select = document.getElementById('wifiSelect');
      select.style.display = 'block';
      select.innerHTML = '<option value="">掃描中，請稍候...</option>';
      fetch('/api/wifi/scan')
        .then(r => r.json())
        .then(list => {
          select.innerHTML = '<option value="">-- 請選擇 Wi-Fi 熱點 --</option>';
          list.forEach(item => {
            if (item.ssid) {
              const opt = document.createElement('option');
              opt.value = item.ssid;
              opt.innerText = item.ssid + ' (' + item.rssi + 'dBm)';
              select.appendChild(opt);
            }
          });
        })
        .catch(() => { select.innerHTML = '<option value="">掃描失敗，請手動輸入</option>'; });
    }

    function selectSsid(sel) {
      if (sel.value) document.getElementById('wifiSsid').value = sel.value;
    }

    function connectWifi() {
      const ssid = document.getElementById('wifiSsid').value.trim();
      const pass = document.getElementById('wifiPass').value;
      if (!ssid) return alert('請輸入或選擇 Wi-Fi 名稱！');
      const box = document.getElementById('wifiStatusBox');
      box.innerHTML = '<div style="color:var(--cyan);">正在嘗試連線至 ' + ssid + '... 請稍候 10 秒後檢查狀態</div>';
      const body = new URLSearchParams();
      body.append('ssid', ssid);
      body.append('pass', pass);
      fetch('/api/wifi/connect', { method: 'POST', body: body })
        .then(r => r.json())
        .then(() => { setTimeout(checkWifiStatus, 6000); })
        .catch(() => alert('發送連線指令失敗'));
    }

    function setCloudUrl(type) {
      const input = document.getElementById('cloudOtaUrl');
      if (type === 'drive') {
        input.value = 'https://drive.google.com/uc?export=download&id=1QwJxZ1gZpncdnnJL5c4IYgl0H-iliAxm';
      } else {
        input.value = 'https://raw.githubusercontent.com/eric-aicode/aiot/main/Project_10SecChallenge/firmware_10SecChallenge.bin';
      }
    }

    function startCloudOTA() {
      const url = document.getElementById('cloudOtaUrl').value.trim();
      if (!url) return alert('請輸入韌體下載網址！');
      if (!confirm('即將從雲端下載並更新韌體，更新期間請勿拔除電源。確定繼續？')) return;
      const body = new URLSearchParams();
      body.append('url', url);
      fetch('/api/ota/cloud', { method: 'POST', body: body })
        .then(r => r.text())
        .then(msg => alert(msg))
        .catch(err => alert('啟動失敗: ' + err));
    }

    // 排行榜機制 (區分兩個模式)
    function addRecord(mode, p1, p2, rank) {
      const key = mode === 'timelord' ? 'timer10s_lb' : 'tapper10s_lb';
      let lb = JSON.parse(localStorage.getItem(key) || '[]');
      if (mode === 'timelord') {
        lb.push({
          time: (p1 / 1000).toFixed(3),
          delta: (p2 / 1000).toFixed(3),
          absDelta: Math.abs(p2),
          rank: rank,
          timeStr: new Date().toLocaleTimeString('zh-TW', { hour12: false })
        });
        lb.sort((a,b) => a.absDelta - b.absDelta);
      } else {
        lb.push({
          hits: p1,
          cps: p2,
          rank: rank,
          timeStr: new Date().toLocaleTimeString('zh-TW', { hour12: false })
        });
        lb.sort((a,b) => b.hits - a.hits); // 按次數高到低
      }
      if (lb.length > 5) lb = lb.slice(0, 5);
      localStorage.setItem(key, JSON.stringify(lb));
    }

    function renderDrawer() {
      const key = currentGameMode === MODE_TIME_LORD ? 'timer10s_lb' : 'tapper10s_lb';
      let lb = JSON.parse(localStorage.getItem(key) || '[]');
      drawerTitle.innerText = currentGameMode === MODE_TIME_LORD ? '🏆 今日停秒榜 (Top 5)' : '⚡ 今日手速榜 (Top 5)';

      if (lb.length === 0) {
        drawerRecords.innerHTML = '<div style="text-align:center; color:var(--muted); padding:20px;">尚無紀錄，快拍下按鈕挑戰！</div>';
        return;
      }
      let html = '';
      lb.forEach((r, idx) => {
        const medal = idx === 0 ? '🥇' : idx === 1 ? '🥈' : idx === 2 ? '🥉' : `#${idx+1}`;
        if (currentGameMode === MODE_TIME_LORD) {
          const color = r.delta == 0 ? 'var(--green)' : r.delta < 0 ? 'var(--cyan)' : 'var(--red)';
          const sign = r.delta > 0 ? '+' : '';
          html += `
            <div class="record-item">
              <span style="font-weight:900; color:var(--gold); width:28px;">${medal}</span>
              <span style="font-family:'Orbitron'; font-weight:700;">${r.time}s</span>
              <span style="font-family:'Orbitron'; color:${color}; font-size:0.85rem;">${sign}${r.delta}s</span>
              <span style="font-size:0.75rem; color:var(--muted);">${r.timeStr}</span>
            </div>
          `;
        } else {
          html += `
            <div class="record-item">
              <span style="font-weight:900; color:var(--gold); width:28px;">${medal}</span>
              <span style="font-family:'Orbitron'; font-weight:900; color:var(--green);">${r.hits} 下</span>
              <span style="font-family:'Orbitron'; font-size:0.85rem; color:var(--cyan);">${r.cps} 次/秒</span>
              <span style="font-size:0.75rem; color:var(--muted);">${r.timeStr}</span>
            </div>
          `;
        }
      });
      drawerRecords.innerHTML = html;
    }

    function clearLeaderboard() {
      const key = currentGameMode === MODE_TIME_LORD ? 'timer10s_lb' : 'tapper10s_lb';
      if (confirm('確定清空當前模式英雄榜嗎？')) {
        localStorage.removeItem(key);
        renderDrawer();
      }
    }

    function setupSSE() {
      const evt = new EventSource('/events');
      evt.onmessage = function(e) {
        try { applyState(JSON.parse(e.data)); } catch(err){}
      };
      evt.onerror = function() {
        setTimeout(setupSSE, 2000);
      };
    }

    window.onload = function() {
      applyFont();
      checkWifiStatus();
      setupSSE();
    };
  </script>
</body>
</html>
)rawliteral";

#endif
