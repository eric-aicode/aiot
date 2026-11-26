#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <Arduino.h>

const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-TW">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>ArenaPod 五合一單鍵競技場</title>
  <link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@700&family=Orbitron:wght@700;900&family=Share+Tech+Mono&family=Roboto+Mono:wght@700&display=swap" media="print" onload="this.media='all'">
  <noscript><link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@700&family=Orbitron:wght@700;900&family=Share+Tech+Mono&family=Roboto+Mono:wght@700&display=swap"></noscript>
  <style>
    :root {
      --bg: #050811;
      --cyan: #00f2fe;
      --green: #00ff87;
      --gold: #ffb703;
      --red: #ff0055;
      --purple: #a855f7;
      --orange: #ff7b00;
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
      15%, 85% { transform: translate3d(-5px, 0, 0); }
      30%, 70% { transform: translate3d(6px, 0, 0); }
      45%, 60% { transform: translate3d(-7px, 0, 0); }
    }

    .celebration-halo {
      position: fixed;
      inset: 0;
      pointer-events: none;
      z-index: 70;
      opacity: 0;
    }
    .halo-perfect {
      background: radial-gradient(circle, rgba(0,255,135,0.5) 0%, rgba(255,183,3,0.3) 45%, transparent 70%);
      opacity: 1;
      animation: haloFade 1s ease-out forwards;
    }
    .halo-godlike {
      background: radial-gradient(circle, rgba(0,242,254,0.5) 0%, rgba(0,255,135,0.2) 50%, transparent 70%);
      opacity: 1;
      animation: haloFade 1s ease-out forwards;
    }
    .halo-master {
      background: radial-gradient(circle, rgba(255,183,3,0.4) 0%, transparent 60%);
      opacity: 1;
      animation: haloFade 1s ease-out forwards;
    }
    .halo-miss {
      background: radial-gradient(circle, rgba(255,0,85,0.45) 0%, transparent 60%);
      opacity: 1;
      animation: haloFade 1s ease-out forwards;
    }
    .halo-bang {
      background: rgba(0, 255, 135, 0.45);
      opacity: 1;
      animation: haloFlash 0.3s ease-out forwards;
    }
    @keyframes haloFade {
      0% { transform: scale(0.65); opacity: 0.95; }
      100% { transform: scale(1.4); opacity: 0; }
    }
    @keyframes haloFlash {
      0% { opacity: 1; }
      100% { opacity: 0; }
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
      background: rgba(15, 23, 42, 0.85);
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

    .btn-mode {
      background: linear-gradient(135deg, rgba(168,85,247,0.4) 0%, rgba(0,242,254,0.4) 100%);
      border-color: var(--cyan);
      color: #fff;
    }
    .btn-fs {
      background: linear-gradient(135deg, rgba(255,183,3,0.35) 0%, rgba(251,133,0,0.35) 100%);
      border-color: var(--gold);
      color: var(--gold);
    }
    .btn-wifi.online {
      border-color: var(--green);
      color: var(--green);
      background: rgba(0, 255, 135, 0.15);
    }

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
      text-align: center;
    }
    .target-banner strong {
      color: var(--gold);
      font-family: 'Orbitron', monospace;
      font-size: 1.25em;
    }

    /* 👑 超極限巨幕數字 (全螢幕最大化，加大字距防黏字) */
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

    /* 單一超巨大字體 (手速次數 / 閃電反應時間 / 炸彈分數) */
    .single-huge-digits {
      font-size: min(34vw, 38vh);
      font-weight: 900;
      line-height: 0.95;
      color: var(--cyan);
      text-shadow: 0 0 55px rgba(0, 242, 254, 0.75), 0 0 110px rgba(0, 242, 254, 0.35);
      cursor: pointer;
      font-family: 'Orbitron', 'Impact', sans-serif;
      text-align: center;
    }

    /* 4 款零震動字體樣式 */
    .font-sci-fi { font-family: 'Orbitron', 'Chakra Petch', 'Trebuchet MS', sans-serif; letter-spacing: 0.02em; }
    .font-impact { font-family: 'Impact', 'Arial Black', sans-serif; letter-spacing: 0.04em; }
    .font-7seg   { font-family: 'Share Tech Mono', 'Courier New', monospace; letter-spacing: 0.05em; }
    .font-modern { font-family: 'Roboto Mono', 'SF Pro Mono', 'Consolas', monospace; letter-spacing: 0.03em; }

    .idle { color: #64748b !important; text-shadow: none !important; }
    .running { color: var(--cyan); text-shadow: 0 0 50px rgba(0, 242, 254, 0.7); }
    .perfect { color: var(--green) !important; text-shadow: 0 0 65px rgba(0, 255, 135, 0.95) !important; }
    .warning { color: var(--gold) !important; text-shadow: 0 0 50px rgba(255, 183, 3, 0.8) !important; }
    .danger  { color: var(--red) !important; text-shadow: 0 0 60px rgba(255, 0, 85, 0.9) !important; }

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
    .rank-tag.danger {
      background: rgba(255, 0, 85, 0.25);
      border-color: var(--red);
      color: var(--red);
      box-shadow: 0 0 25px rgba(255, 0, 85, 0.5);
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
      <button class="pill-btn btn-mode" id="btnMode" onclick="cycleGameMode()">
        🎮 模式: ⏱️ 十秒節拍停秒
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
    <div class="target-banner" id="targetBanner">
      🎯 目標時間 <strong>10.000 秒</strong>
    </div>

    <!-- 視圖 0：十秒停秒挑戰 (Time Lord) -->
    <div class="giant-digits font-sci-fi idle" id="boxTimeLord">
      <span class="d-slot" id="d0">0</span><span class="d-slot" id="d1">0</span><span class="d-dot">.</span><span class="d-slot" id="d2">0</span><span class="d-slot" id="d3">0</span><span class="d-slot" id="d4">0</span>
    </div>

    <!-- 視圖 1 / 2：單一超巨大數字 (手速 / 閃電) -->
    <div class="single-huge-digits font-sci-fi idle" id="boxSingleDigits" style="display:none;">
      0
    </div>

    <!-- 視圖 3：🦖 經典單鍵離線小恐龍 (Chrome Dino Runner) -->
    <div id="boxDino" style="display:none; width:100%; max-width:820px; flex-direction:column; align-items:center; gap:8px;">
      <div style="width:100%; display:flex; justify-content:space-between; align-items:center; font-family:'Share Tech Mono', 'Courier New', monospace; font-size:clamp(1.1rem, 2.5vw, 1.8rem); font-weight:700; color:#535353; padding:0 12px; letter-spacing:2px;">
        <span style="color:#757575;">HI <strong id="dinoHighScore" style="color:#d1d5db;">00000</strong></span>
        <span id="dinoCurrentScore" style="color:#f3f4f6; font-weight:900;">00000</span>
      </div>
      <canvas id="dinoCanvas" width="800" height="240" style="width:100%; height:auto; background:#202124; border:1px solid #535353; border-radius:10px; box-shadow:0 8px 30px rgba(0,0,0,0.7);"></canvas>
    </div>

    <!-- 視圖 4：🏎️ 懸崖勒馬極限煞車 (Cliff Brake Challenge) -->
    <div id="boxCliff" style="display:none; width:100%; max-width:820px; flex-direction:column; align-items:center; gap:8px;">
      <div style="width:100%; display:grid; grid-template-columns:140px 1fr 140px; align-items:center; font-family:'Share Tech Mono', 'Courier New', monospace; font-size:clamp(0.9rem, 2vw, 1.25rem); font-weight:700; color:var(--muted); padding:0 8px;">
        <span style="text-align:left;">🏎️ 車速: <strong id="cliffSpeedDisplay" style="color:var(--gold); font-family:'Share Tech Mono', monospace; font-variant-numeric:tabular-nums; display:inline-block; min-width:40px; text-align:right;">0</strong> km/h</span>
        <span style="text-align:center; color:var(--text); font-weight:800; white-space:nowrap; overflow:hidden;" id="cliffStatusBanner">第 1 拍起跑狂飆 · 第 2 拍死命急煞！</span>
        <span style="text-align:right;">🏆 最佳: <strong id="cliffBestDisplay" style="color:var(--cyan); font-family:'Share Tech Mono', monospace; font-variant-numeric:tabular-nums; display:inline-block; min-width:55px; text-align:right;">--</strong></span>
      </div>
      <canvas id="cliffCanvas" width="800" height="260" style="width:100%; height:auto; background:#0b0f19; border:2px solid var(--border); border-radius:12px; box-shadow:0 10px 30px rgba(0,0,0,0.8); touch-action:none; display:block;"></canvas>
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
        🚀 韌體無線升級中心 (OTA)
      </div>

      <!-- 即時雙向進度條容器 -->
      <div id="otaProgressContainer" style="display:none; flex-direction:column; gap:6px; background:rgba(0,0,0,0.6); border:1px solid var(--cyan); border-radius:10px; padding:12px; margin-top:4px;">
        <div style="display:flex; justify-content:space-between; font-size:0.82rem; font-weight:700;">
          <span id="otaStageText" style="color:var(--cyan);">正在下載韌體...</span>
          <span id="otaPercentText" style="color:var(--gold);">0%</span>
        </div>
        <div style="width:100%; height:12px; background:rgba(255,255,255,0.1); border-radius:6px; overflow:hidden;">
          <div id="otaProgressBar" style="width:0%; height:100%; background:linear-gradient(90deg, #00f2fe, #00ff87); transition:width 0.15s ease;"></div>
        </div>
        <div id="otaDetailText" style="font-size:0.75rem; color:var(--muted); text-align:center;">準備中...</div>
      </div>

      <!-- 方式一：雲端直連一鍵升級 -->
      <div class="form-group" id="otaCloudSection" style="margin-top:6px;">
        <span class="form-label">方式 ①：GitHub 雲端一鍵下載寫入</span>
        <div style="display:flex; gap:6px; margin-bottom:4px;">
          <button class="pill-btn" style="padding:3px 8px; font-size:0.75rem;" onclick="setCloudUrl('github')">🐙 GitHub 載點 (推薦)</button>
          <button class="pill-btn" style="padding:3px 8px; font-size:0.75rem;" onclick="setCloudUrl('drive')">📁 Google Drive 載點</button>
        </div>
        <input type="text" class="form-input" id="cloudOtaUrl" value="https://raw.githubusercontent.com/eric-aicode/aiot/main/Project_10SecChallenge/firmware_10SecChallenge.bin">
        <button class="pill-btn" id="btnStartCloudOTA" style="justify-content:center; background:linear-gradient(135deg,#ffb703,#fb8500); color:#000; font-weight:900; margin-top:4px;" onclick="startSmartCloudOTA()">
          ☁️ 從雲端一鍵升級 (即時進度條)
        </button>
      </div>

      <!-- 方式二：手動本機檔案上傳 -->
      <div class="form-group" id="otaLocalSection" style="margin-top:10px;">
        <span class="form-label">方式 ②：直接選取本機 .bin 檔案寫入</span>
        <div style="display:flex; gap:6px;">
          <input type="file" id="localBinFile" accept=".bin" style="display:none;" onchange="handleFileChosen(this)">
          <button class="pill-btn" style="flex:1; justify-content:center;" onclick="document.getElementById('localBinFile').click()">
            📁 <span id="localFileName">選擇電腦上的 .bin 檔案...</span>
          </button>
          <button class="pill-btn" id="btnUploadLocal" style="background:var(--green); color:#000; font-weight:900; display:none;" onclick="uploadLocalFile()">
            🚀 寫入
          </button>
        </div>
      </div>

      <a href="/update" class="pill-btn" style="margin-top:auto; justify-content:center; font-size:0.75rem; color:var(--muted);">
        ⚙️ 原生備用升級後台 (/update)
      </a>
    </div>
  </div>

  <script>
    const STATE_READY = 0;
    const STATE_RUNNING = 1;
    const STATE_RESULT = 2;

    const MODES = [
      { id: 0, name: '⏱️ 十秒節拍停秒', hint: '🎯 目標時間 <strong>10.000 秒</strong>', lbKey: 'timer10s_lb', lbTitle: '🏆 今日停秒榜 (Top 5)' },
      { id: 1, name: '⚡ 十秒手速狂拍', hint: '⚡ 倒數 <strong>10.0 秒</strong> 瘋狂狂拍！', lbKey: 'tapper10s_lb', lbTitle: '⚡ 今日手速榜 (Top 5)' },
      { id: 2, name: '🤠 閃電神經對決', hint: '⚠️ 屏息等待信號... <strong>見綠光暴閃瞬間拔槍！</strong>', lbKey: 'lightning10s_lb', lbTitle: '🤠 閃電牛仔榜 (Top 5)' },
      { id: 3, name: '🦖 經典單鍵小恐龍', hint: '🦖 拍擊實體按鈕 <strong>跳躍閃避障礙物！</strong>', lbKey: 'dino10s_lb', lbTitle: '🦖 恐龍跑酷榜 (Top 5)' },
      { id: 4, name: '🏎️ 懸崖勒馬極限煞車', hint: '🏎️ 第 1 拍<strong>油門狂飆</strong>！第 2 拍<strong>死命極限踩煞車！</strong>', lbKey: 'cliff10s_lb', lbTitle: '🏎️ 懸崖車神榜 (Top 5)' }
    ];
    let currentGameMode = 0;

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
    const boxSingleDigits = document.getElementById('boxSingleDigits');

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
        boxSingleDigits.classList.remove(f.id);
      });
      boxTimeLord.classList.add(FONTS[fontIndex].id);
      boxSingleDigits.classList.add(FONTS[fontIndex].id);
      btnFont.innerText = '🔤 字體: ' + FONTS[fontIndex].name;
      localStorage.setItem('timer10s_font', fontIndex.toString());
    }
    function cycleFont() {
      fontIndex = (fontIndex + 1) % FONTS.length;
      applyFont();
    }

    // 輪流切換 4 種遊戲模式
    function cycleGameMode() {
      const nextMode = (currentGameMode + 1) % MODES.length;
      fetch('/api/mode?set=' + nextMode, { method: 'POST' })
        .then(() => switchUIMode(nextMode))
        .catch(() => switchUIMode(nextMode));
    }

    function switchUIMode(mode) {
      currentGameMode = mode;
      currentState = STATE_READY;
      if (animFrameId) cancelAnimationFrame(animFrameId);
      if (dinoLoopId) cancelAnimationFrame(dinoLoopId);

      const m = MODES[mode];
      btnMode.innerHTML = '🎮 模式: ' + m.name;
      targetBanner.innerHTML = m.hint;
      drawerTitle.innerText = m.lbTitle;

      const resultOverlay = document.querySelector('.result-overlay');
      const boxCliff = document.getElementById('boxCliff');

      if (mode === 0) {
        // 模式 0：十秒停秒 (Time Lord)
        boxTimeLord.style.display = 'inline-flex';
        boxSingleDigits.style.display = 'none';
        boxDino.style.display = 'none';
        if (boxCliff) boxCliff.style.display = 'none';
        resultOverlay.style.display = 'flex';
        updateDigitSlots(0);
        boxTimeLord.className = 'giant-digits ' + FONTS[fontIndex].id + ' idle';
      } else if (mode === 1 || mode === 2) {
        // 模式 1 / 2：單一超巨大字體
        boxTimeLord.style.display = 'none';
        boxSingleDigits.style.display = 'block';
        boxDino.style.display = 'none';
        if (boxCliff) boxCliff.style.display = 'none';
        resultOverlay.style.display = 'flex';
        boxSingleDigits.className = 'single-huge-digits ' + FONTS[fontIndex].id + ' idle';

        if (mode === 1) boxSingleDigits.innerText = '0';
        else if (mode === 2) boxSingleDigits.innerText = '--.---';
      } else if (mode === 3) {
        // 模式 3：🦖 經典單鍵離線小恐龍
        boxTimeLord.style.display = 'none';
        boxSingleDigits.style.display = 'none';
        boxDino.style.display = 'flex';
        if (boxCliff) boxCliff.style.display = 'none';
        resultOverlay.style.display = 'none';
        initDinoGame();
      } else if (mode === 4) {
        // 模式 4：🏎️ 懸崖勒馬極限煞車
        boxTimeLord.style.display = 'none';
        boxSingleDigits.style.display = 'none';
        boxDino.style.display = 'none';
        if (boxCliff) boxCliff.style.display = 'flex';
        resultOverlay.style.display = 'none';
        initCliffGame();
      }

      deltaDisplay.innerText = '--';
      deltaDisplay.className = 'delta-display';
      rankTag.innerText = '準備就緒';
      rankTag.className = 'rank-tag';
      bottomHint.className = 'bottom-hint';
      bottomHint.innerHTML = (mode === 3) ? '<span>🦖 拍擊實體大按鈕起跑跳躍！</span>' :
                             (mode === 4) ? '<span>🏎️ 第 1 拍起跑狂飆 · 第 2 拍急煞！</span>' :
                             '<span>🔘 拍擊實體大按鈕開始！</span>';
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

    // Web Audio
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

    function playExplosion() {
      if (!soundEnabled) return;
      playTone(150, 'sawtooth', 0.5, 0.6);
      setTimeout(() => playTone(80, 'square', 0.6, 0.7), 80);
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
        : tier === 'boom'
        ? ['#ff0055', '#ff7b00', '#ffd166', '#ffffff']
        : ['#ffb703', '#ffd166'];

      const count = tier === 'perfect' ? 80 : (tier === 'boom' ? 100 : 45);
      const centerX = window.innerWidth / 2;
      const centerY = window.innerHeight / 2;

      for (let i = 0; i < count; i++) {
        const angle = Math.random() * Math.PI * 2;
        const speed = Math.random() * (tier === 'boom' ? 16 : 12) + 4;
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
      if (currentGameMode === 3) {
        dinoJump();
      } else if (currentGameMode === 4) {
        handleCliffTap();
      }
      fetch('/api/button', { method: 'POST' }).catch(()=>{});
    }

    // 本地平滑渲染
    function runLocalClock() {
      if (animFrameId) cancelAnimationFrame(animFrameId);
      function frame() {
        if (currentState === STATE_RUNNING) {
          const elapsed = performance.now() - localStartTime;

          if (currentGameMode === 0) {
            // 模式 0：十秒停秒
            updateDigitSlots(elapsed);
          } else if (currentGameMode === 1) {
            // 模式 1：手速倒數
            const remainMs = Math.max(0, 10000 - elapsed);
            targetBanner.innerHTML = `⚡ 剩餘時間 <strong>${(remainMs/1000).toFixed(1)}s</strong> 瘋狂狂拍！`;
          } else if (currentGameMode === 2) {
            // 模式 2：閃電對決 (等待信號時不動，信號響起時急速狂跳)
            if (boxSingleDigits.classList.contains('signaled')) {
              const reactTime = (performance.now() - signalStartTime) / 1000;
              boxSingleDigits.innerText = reactTime.toFixed(3) + 's';
            }
          }
          animFrameId = requestAnimationFrame(frame);
        }
      }
      animFrameId = requestAnimationFrame(frame);
    }

    let signalStartTime = 0;

    // 狀態機處理：由後端事件驅動
    function applyState(data) {
      if (audioCtx.state === 'suspended') audioCtx.resume();

      if (data.mode !== undefined && data.mode !== currentGameMode) {
        switchUIMode(data.mode);
      }

      if (data.type === 'START') {
        currentState = STATE_RUNNING;
        localStartTime = performance.now();

        if (currentGameMode === 0) {
          boxTimeLord.className = 'giant-digits ' + FONTS[fontIndex].id + ' running';
          bottomHint.className = 'bottom-hint';
          bottomHint.innerHTML = '<span>⚡ 計時狂飆中！第 2 下拍擊定格！</span>';
        } else if (currentGameMode === 1) {
          currentHits = data.h || 1;
          boxSingleDigits.innerText = currentHits;
          boxSingleDigits.className = 'single-huge-digits ' + FONTS[fontIndex].id + ' running';
          bottomHint.className = 'bottom-hint';
          bottomHint.innerHTML = '<span>⚡ 倒數 10 秒！火速狂拍按鍵！！！</span>';
        } else if (currentGameMode === 2) {
          // 閃電對決進入屏息等待
          boxSingleDigits.innerText = 'WAIT...';
          boxSingleDigits.className = 'single-huge-digits ' + FONTS[fontIndex].id + ' warning';
          targetBanner.innerHTML = '⚠️ 屏息凝神... <strong>搶拍偷跑直接出局！</strong>';
          bottomHint.className = 'bottom-hint locked';
          bottomHint.innerHTML = '<span>⚠️ 等待綠光信號，千萬別搶拍！</span>';
        } else if (currentGameMode === 3) {
          // 小恐龍起跑
          startDinoGame();
        } else if (currentGameMode === 4) {
          // 懸崖勒馬起跑狂飆
          startCliffGame();
        }

        deltaDisplay.innerText = '--';
        deltaDisplay.className = 'delta-display';
        rankTag.innerText = '挑戰進行中...';
        rankTag.className = 'rank-tag';
        playTone(950, 'sine', 0.1, 0.4);
        runLocalClock();
      }
      else if (data.type === 'LIGHTNING_SIGNAL') {
        // 模式 2 綠光信號暴閃！BANG!
        signalStartTime = performance.now();
        boxSingleDigits.classList.add('signaled');
        boxSingleDigits.className = 'single-huge-digits ' + FONTS[fontIndex].id + ' perfect';
        targetBanner.innerHTML = '⚡⚡ <strong>拔槍！BANG！火速拍擊！</strong> ⚡⚡';
        haloLayer.className = 'celebration-halo halo-bang';
        playTone(1800, 'square', 0.15, 0.6);
      }
      else if (data.type === 'HIT') {
        currentHits = (data.h !== undefined) ? data.h : data.hits;
        boxSingleDigits.innerText = currentHits;
        playTone(1000 + Math.min(currentHits * 15, 800), 'sine', 0.03, 0.2);
      }
      else if (data.type === 'STOP') {
        currentState = STATE_RESULT;
        if (animFrameId) cancelAnimationFrame(animFrameId);
        boxSingleDigits.classList.remove('signaled');

        haloLayer.className = 'celebration-halo';
        void haloLayer.offsetWidth;

        if (currentGameMode === 0) {
          // 模式 0 停秒結算
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
          } else {
            const deltaSec = (absDelta / 1000).toFixed(3);
            const sign = delta < 0 ? '-' : '+';
            deltaDisplay.innerText = `${sign}${deltaSec}s (${delta < 0 ? '快了' : '慢了'})`;
            deltaDisplay.className = absDelta <= 50 ? 'delta-display delta-zero' : (delta < 0 ? 'delta-display delta-fast' : 'delta-display delta-slow');
            boxTimeLord.className = 'giant-digits ' + FONTS[fontIndex].id;
            rankTag.className = 'rank-tag';
            haloLayer.classList.add(absDelta <= 50 ? 'halo-godlike' : 'halo-master');
            triggerCelebrationFX(absDelta <= 50 ? 'godlike' : 'master');
            playTone(550, 'triangle', 0.2, 0.35);
          }
          rankTag.innerText = rank;
          addRecord(0, elapsed, delta, rank);
        }
        else if (currentGameMode === 1) {
          // 模式 1 手速結算
          const hits = data.hits;
          const cps = (data.cps || (hits / 10.0)).toFixed(1);
          const rank = data.rank;
          boxSingleDigits.innerText = hits;
          targetBanner.innerHTML = '⚡ 時間到！<strong>手速挑戰完成</strong>';
          deltaDisplay.innerText = `${hits} 下 (平均 ${cps} 次/秒)`;
          deltaDisplay.className = 'delta-display delta-zero';
          rankTag.innerText = rank;

          if (hits >= 75) {
            boxSingleDigits.className = 'single-huge-digits ' + FONTS[fontIndex].id + ' perfect';
            rankTag.className = 'rank-tag perfect';
            haloLayer.classList.add('halo-perfect');
            document.body.classList.add('screen-shake');
            setTimeout(() => document.body.classList.remove('screen-shake'), 350);
            triggerCelebrationFX('perfect');
            playFanfare();
          } else {
            boxSingleDigits.className = 'single-huge-digits ' + FONTS[fontIndex].id;
            haloLayer.classList.add('halo-godlike');
            triggerCelebrationFX('godlike');
            playTone(660, 'triangle', 0.25, 0.4);
          }
          addRecord(1, hits, cps, rank);
        }
        else if (currentGameMode === 2) {
          // 模式 2 閃電對決結算
          if (data.fault) {
            boxSingleDigits.innerText = 'FOUL!';
            boxSingleDigits.className = 'single-huge-digits ' + FONTS[fontIndex].id + ' danger';
            targetBanner.innerHTML = '❌ <strong>搶拍違規！信號未響即出手！</strong>';
            deltaDisplay.innerText = '偷跑出局 (0 分)';
            deltaDisplay.className = 'delta-display delta-slow';
            rankTag.innerText = '走火出局 (FAULT)';
            rankTag.className = 'rank-tag danger';
            haloLayer.classList.add('halo-miss');
            playTone(200, 'sawtooth', 0.25, 0.5);

            bottomHint.className = 'bottom-hint wait-reset';
            bottomHint.innerHTML = '<span>❌ 搶拍偷跑！拍擊按鈕立即重來！</span>';
            return; // 立即返回，不鎖定 1 秒！
          } else {
            const reactionMs = data.time;
            const reactSec = (reactionMs / 1000).toFixed(3);
            const rank = data.rank;
            boxSingleDigits.innerText = reactSec + 's';
            targetBanner.innerHTML = '🤠 <strong>拔槍成功！神經極限！</strong>';
            deltaDisplay.innerText = `反應時間: ${reactionMs} 毫秒`;
            deltaDisplay.className = 'delta-display delta-zero';
            rankTag.innerText = rank;

            if (reactionMs <= 220) {
              boxSingleDigits.className = 'single-huge-digits ' + FONTS[fontIndex].id + ' perfect';
              rankTag.className = 'rank-tag perfect';
              haloLayer.classList.add('halo-perfect');
              document.body.classList.add('screen-shake');
              setTimeout(() => document.body.classList.remove('screen-shake'), 350);
              triggerCelebrationFX('perfect');
              playFanfare();
            } else {
              boxSingleDigits.className = 'single-huge-digits ' + FONTS[fontIndex].id;
              haloLayer.classList.add('halo-godlike');
              triggerCelebrationFX('godlike');
              playTone(700, 'triangle', 0.2, 0.4);
            }
            addRecord(2, reactionMs, reactSec, rank);
          }
        }

        bottomHint.className = 'bottom-hint locked';
        bottomHint.innerHTML = '<span>⚡ 定格結算中... (1s)</span>';
      }
      else if (data.type === 'DINO_JUMP') {
        if (currentGameMode === 3) {
          dinoJump();
        }
      }
      else if (data.type === 'BRAKE') {
        if (currentGameMode === 4) {
          brakeCliffGame();
        }
      }
      else if (data.type === 'CLIFF_TIMEOUT') {
        if (currentGameMode === 4) {
          cliffTimeout();
        }
      }
      else if (data.type === 'WAIT_RESET') {
        bottomHint.className = 'bottom-hint wait-reset';
        bottomHint.innerHTML = '<span>🔄 拍擊按鈕 歸零準備</span>';
      }
      else if (data.type === 'RESET') {
        currentState = STATE_READY;
        if (animFrameId) cancelAnimationFrame(animFrameId);
        switchUIMode(currentGameMode);
      }
    }

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

    function handleFileChosen(input) {
      if (input.files && input.files[0]) {
        const file = input.files[0];
        document.getElementById('localFileName').innerText = file.name + ` (${(file.size/1024).toFixed(0)} KB)`;
        document.getElementById('btnUploadLocal').style.display = 'inline-flex';
      }
    }

    function uploadBinaryToESP(blob, filename = 'firmware.bin') {
      const pContainer = document.getElementById('otaProgressContainer');
      const stageText = document.getElementById('otaStageText');
      const percentText = document.getElementById('otaPercentText');
      const pBar = document.getElementById('otaProgressBar');
      const detailText = document.getElementById('otaDetailText');

      pContainer.style.display = 'flex';
      stageText.innerText = '🚀 正在寫入 ESP8266 Flash...';
      stageText.style.color = 'var(--cyan)';
      percentText.innerText = '0%';
      pBar.style.width = '0%';
      detailText.innerText = `準備傳輸 ${(blob.size / 1024).toFixed(1)} KB...`;

      const formData = new FormData();
      formData.append('firmware', blob, filename);

      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/update');

      xhr.upload.onprogress = function(e) {
        if (e.lengthComputable) {
          const pct = Math.round((e.loaded / e.total) * 100);
          pBar.style.width = pct + '%';
          percentText.innerText = pct + '%';
          detailText.innerText = `已寫入 ${(e.loaded / 1024).toFixed(1)} / ${(e.total / 1024).toFixed(1)} KB`;
        }
      };

      xhr.onload = function() {
        if (xhr.status === 200) {
          pBar.style.width = '100%';
          percentText.innerText = '100%';
          stageText.innerText = '✅ 韌體升級完成！';
          stageText.style.color = 'var(--green)';

          let count = 5;
          detailText.innerHTML = `<strong style="color:var(--gold);">板子正在重開機！${count} 秒後自動重新整理網頁...</strong>`;
          const timer = setInterval(() => {
            count--;
            if (count <= 0) {
              clearInterval(timer);
              window.location.reload();
            } else {
              detailText.innerHTML = `<strong style="color:var(--gold);">板子正在重開機！${count} 秒後自動重新整理網頁...</strong>`;
            }
          }, 1000);
        } else {
          stageText.innerText = '❌ 寫入失敗: HTTP ' + xhr.status;
          stageText.style.color = 'var(--red)';
          detailText.innerText = xhr.responseText || '傳輸被中斷或板子拒絕寫入';
        }
      };

      xhr.onerror = function() {
        stageText.innerText = '❌ 寫入連線失敗！';
        stageText.style.color = 'var(--red)';
        detailText.innerText = '請確認板子未斷電且 Wi-Fi 連線正常';
      };

      xhr.send(formData);
    }

    function uploadLocalFile() {
      const fileInput = document.getElementById('localBinFile');
      if (!fileInput.files || !fileInput.files[0]) return alert('請先選擇 .bin 檔案！');
      if (!confirm(`即將將「${fileInput.files[0].name}」寫入板子，過程中請勿拔除電源。確定繼續？`)) return;
      uploadBinaryToESP(fileInput.files[0], fileInput.files[0].name);
    }

    function startSmartCloudOTA() {
      const url = document.getElementById('cloudOtaUrl').value.trim();
      if (!url) return alert('請輸入韌體下載網址！');
      if (!confirm('即將從雲端下載最新韌體並即時寫入板子，過程中請勿關閉網頁或拔除電源。確定繼續？')) return;

      const pContainer = document.getElementById('otaProgressContainer');
      const stageText = document.getElementById('otaStageText');
      const percentText = document.getElementById('otaPercentText');
      const pBar = document.getElementById('otaProgressBar');
      const detailText = document.getElementById('otaDetailText');

      pContainer.style.display = 'flex';
      stageText.innerText = '📥 正在從雲端載入韌體檔案...';
      stageText.style.color = 'var(--cyan)';
      percentText.innerText = '連線中...';
      pBar.style.width = '20%';
      detailText.innerText = '正在連線遠端伺服器 (GitHub)...';

      fetch(url)
        .then(res => {
          if (!res.ok) throw new Error('雲端載點回應 HTTP ' + res.status);
          pBar.style.width = '60%';
          detailText.innerText = '下載成功，正在解碼二進制檔案...';
          return res.blob();
        })
        .then(blob => {
          pBar.style.width = '100%';
          detailText.innerText = `檔案取得成功 (${(blob.size / 1024).toFixed(1)} KB)！即刻啟動 Flash 寫入...`;
          setTimeout(() => {
            uploadBinaryToESP(blob, 'firmware_10SecChallenge.bin');
          }, 300);
        })
        .catch(err => {
          stageText.innerText = '⚠️ 瀏覽器直連受限，切換由板子端下載...';
          stageText.style.color = 'var(--gold)';
          detailText.innerText = '正在請板子後端直接下載: ' + err.message;

          const body = new URLSearchParams();
          body.append('url', url);
          fetch('/api/ota/cloud', { method: 'POST', body: body })
            .then(r => r.text())
            .then(msg => {
              detailText.innerText = msg;
            })
            .catch(e => {
              stageText.innerText = '❌ 雲端更新失敗';
              stageText.style.color = 'var(--red)';
              detailText.innerText = e.message;
            });
        });
    }

    // 4 個模式獨立排行榜
    function addRecord(mode, p1, p2, rank) {
      const key = MODES[mode].lbKey;
      let lb = JSON.parse(localStorage.getItem(key) || '[]');
      const nowStr = new Date().toLocaleTimeString('zh-TW', { hour12: false });

      if (mode === 0) { // 停秒
        lb.push({ time: (p1/1000).toFixed(3), delta: (p2/1000).toFixed(3), absDelta: Math.abs(p2), rank: rank, timeStr: nowStr });
        lb.sort((a,b) => a.absDelta - b.absDelta);
      } else if (mode === 1) { // 手速
        lb.push({ hits: p1, cps: p2, rank: rank, timeStr: nowStr });
        lb.sort((a,b) => b.hits - a.hits);
      } else if (mode === 2) { // 閃電對決 (毫秒越短越好)
        lb.push({ reactMs: p1, reactSec: p2, rank: rank, timeStr: nowStr });
        lb.sort((a,b) => a.reactMs - b.reactMs);
      } else if (mode === 3) { // 離線小恐龍 (得分越高越好)
        lb.push({ score: p1, rank: p2, timeStr: nowStr });
        lb.sort((a,b) => b.score - a.score);
      } else if (mode === 4) { // 懸崖勒馬 (公分越短越好，墜崖排最後)
        lb.push({ distCm: p1, status: p2, rank: rank, timeStr: nowStr });
        lb.sort((a,b) => {
          if (a.status === 'BUSTED' && b.status !== 'BUSTED') return 1;
          if (a.status !== 'BUSTED' && b.status === 'BUSTED') return -1;
          return parseFloat(a.distCm) - parseFloat(b.distCm);
        });
      }

      if (lb.length > 5) lb = lb.slice(0, 5);
      localStorage.setItem(key, JSON.stringify(lb));
    }

    function renderDrawer() {
      const m = MODES[currentGameMode];
      let lb = JSON.parse(localStorage.getItem(m.lbKey) || '[]');
      drawerTitle.innerText = m.lbTitle;

      if (lb.length === 0) {
        drawerRecords.innerHTML = '<div style="text-align:center; color:var(--muted); padding:20px;">尚無紀錄，快拍下按鈕挑戰！</div>';
        return;
      }
      let html = '';
      lb.forEach((r, idx) => {
        const medal = idx === 0 ? '🥇' : idx === 1 ? '🥈' : idx === 2 ? '🥉' : `#${idx+1}`;
        if (currentGameMode === 0) {
          const color = r.delta == 0 ? 'var(--green)' : r.delta < 0 ? 'var(--cyan)' : 'var(--red)';
          const sign = r.delta > 0 ? '+' : '';
          html += `
            <div class="record-item">
              <span style="font-weight:900; color:var(--gold); width:28px;">${medal}</span>
              <span style="font-family:'Orbitron'; font-weight:700;">${r.time}s</span>
              <span style="font-family:'Orbitron'; color:${color}; font-size:0.85rem;">${sign}${r.delta}s</span>
              <span style="font-size:0.75rem; color:var(--muted);">${r.timeStr}</span>
            </div>`;
        } else if (currentGameMode === 1) {
          html += `
            <div class="record-item">
              <span style="font-weight:900; color:var(--gold); width:28px;">${medal}</span>
              <span style="font-family:'Orbitron'; font-weight:900; color:var(--green);">${r.hits} 下</span>
              <span style="font-family:'Orbitron'; font-size:0.85rem; color:var(--cyan);">${r.cps} 次/秒</span>
              <span style="font-size:0.75rem; color:var(--muted);">${r.timeStr}</span>
            </div>`;
        } else if (currentGameMode === 2) {
          html += `
            <div class="record-item">
              <span style="font-weight:900; color:var(--gold); width:28px;">${medal}</span>
              <span style="font-family:'Orbitron'; font-weight:900; color:var(--cyan);">${r.reactSec} 秒</span>
              <span style="font-family:'Orbitron'; font-size:0.85rem; color:var(--gold);">${r.reactMs}ms (${r.rank})</span>
              <span style="font-size:0.75rem; color:var(--muted);">${r.timeStr}</span>
            </div>`;
        } else if (currentGameMode === 3) {
          html += `
            <div class="record-item">
              <span style="font-weight:900; color:var(--gold); width:28px;">${medal}</span>
              <span style="font-family:'Orbitron'; font-weight:900; color:var(--cyan);">${r.score} 分</span>
              <span style="font-family:'Orbitron'; font-size:0.85rem; color:var(--green);">${r.rank}</span>
              <span style="font-size:0.75rem; color:var(--muted);">${r.timeStr}</span>
            </div>`;
        } else if (currentGameMode === 4) {
          const isBusted = r.status === 'BUSTED';
          const distColor = isBusted ? 'var(--red)' : parseFloat(r.distCm) <= 5 ? 'var(--gold)' : 'var(--cyan)';
          const text = isBusted ? '💥 墜崖粉碎' : `${r.distCm} cm`;
          html += `
            <div class="record-item">
              <span style="font-weight:900; color:var(--gold); width:28px;">${medal}</span>
              <span style="font-family:'Orbitron'; font-weight:900; color:${distColor};">${text}</span>
              <span style="font-family:'Orbitron'; font-size:0.85rem; color:var(--green);">${r.rank}</span>
              <span style="font-size:0.75rem; color:var(--muted);">${r.timeStr}</span>
            </div>`;
        }
      });
      drawerRecords.innerHTML = html;
    }

    function clearLeaderboard() {
      const key = MODES[currentGameMode].lbKey;
      if (confirm('確定清空當前模式英雄榜嗎？')) {
        localStorage.removeItem(key);
        renderDrawer();
      }
    }

    // ==========================================================================
    // 🦖 經典單鍵離線小恐龍 (1:1 原版 Google Chrome T-Rex Runner 像素重現)
    // ==========================================================================
    const dinoCanvas = document.getElementById('dinoCanvas');
    const dinoCtx = dinoCanvas ? dinoCanvas.getContext('2d') : null;
    const dinoHighScoreEl = document.getElementById('dinoHighScore');
    const dinoCurrentScoreEl = document.getElementById('dinoCurrentScore');
    const boxDino = document.getElementById('boxDino');

    let dinoHiScore = parseInt(localStorage.getItem('dino10s_hi') || '0', 10);
    let dinoScore = 0;
    let dinoSpeed = 3.6;       // 🐣 起跑初始速度超親民 (由易到難，讓所有人都能上手)
    let isDinoRunning = false;
    let isDinoOver = false;
    let dinoLoopId = null;

    const GROUND_Y = 195;

    // 🦖 Google Chrome 原版 1:1 像素級小恐龍 (44 x 47)
    const dino = {
      x: 55,
      y: GROUND_Y - 47,
      w: 44,
      h: 47,
      vy: 0,
      gravity: 0.58,           // 輕盈自然的重力
      jumpForce: -11.6,         // 完美蓄力滯空跳躍
      isGrounded: true,
      legFrame: 0,
      draw: function(ctx) {
        ctx.save();
        ctx.fillStyle = '#acacac'; // Chrome 原版經典灰

        const x = Math.round(this.x);
        const y = Math.round(this.y);

        // --- 1. 頭部與吻部 ---
        ctx.fillRect(x + 22, y + 0, 20, 2);
        ctx.fillRect(x + 20, y + 2, 24, 2);
        ctx.fillRect(x + 20, y + 4, 24, 8); // 主頭部
        ctx.fillRect(x + 22, y + 12, 22, 2);
        ctx.fillRect(x + 22, y + 14, 16, 2); // 上顎
        ctx.fillRect(x + 22, y + 16, 12, 2);

        // 眼睛
        if (isDinoOver) {
          // 死掉時：經典 Chrome 放大震驚圓眼
          ctx.fillStyle = '#202124';
          ctx.fillRect(x + 24, y + 4, 5, 5);
          ctx.fillStyle = '#acacac';
          ctx.fillRect(x + 25, y + 5, 3, 3);
        } else {
          // 正常眼睛：白瞳＋黑色眼珠
          ctx.fillStyle = '#202124';
          ctx.fillRect(x + 24, y + 4, 4, 4);
          ctx.fillStyle = '#ffffff';
          ctx.fillRect(x + 24, y + 4, 2, 2);
          ctx.fillStyle = '#acacac';
        }

        // 嘴巴開咬牙縫線
        ctx.fillStyle = '#202124';
        ctx.fillRect(x + 36, y + 10, 8, 2);
        ctx.fillStyle = '#acacac';

        // --- 2. 前肢手手 ---
        ctx.fillRect(x + 32, y + 20, 4, 2);
        ctx.fillRect(x + 34, y + 22, 2, 4);

        // --- 3. 軀幹與脊背 ---
        ctx.fillRect(x + 18, y + 14, 4, 4);
        ctx.fillRect(x + 16, y + 16, 4, 4);
        ctx.fillRect(x + 12, y + 18, 18, 14);
        ctx.fillRect(x + 10, y + 20, 20, 10);

        // --- 4. 尾巴 ---
        ctx.fillRect(x + 6, y + 22, 6, 8);
        ctx.fillRect(x + 2, y + 24, 6, 6);
        ctx.fillRect(x + 0, y + 26, 4, 4);

        // --- 5. 雙腳步態 (Chrome 經典 2-Frame 奔跑動畫) ---
        if (!this.isGrounded) {
          // 跳躍空中：雙腿向後縮合
          ctx.fillRect(x + 14, y + 32, 4, 8);
          ctx.fillRect(x + 22, y + 32, 4, 8);
          ctx.fillRect(x + 14, y + 40, 6, 2);
          ctx.fillRect(x + 22, y + 40, 6, 2);
        } else if (isDinoOver) {
          // 死亡定格步態
          ctx.fillRect(x + 14, y + 32, 4, 10);
          ctx.fillRect(x + 14, y + 42, 6, 2);
          ctx.fillRect(x + 24, y + 32, 4, 6);
          ctx.fillRect(x + 24, y + 38, 4, 4);
        } else {
          // 奔跑中：左腳下蹬 vs 右腳下蹬
          if (this.legFrame < 6) {
            // 左腳直立蹬地
            ctx.fillRect(x + 14, y + 32, 4, 13);
            ctx.fillRect(x + 14, y + 45, 6, 2);
            // 右腳收起彎折
            ctx.fillRect(x + 24, y + 32, 4, 6);
            ctx.fillRect(x + 24, y + 38, 4, 2);
          } else {
            // 左腳收起彎折
            ctx.fillRect(x + 14, y + 32, 4, 6);
            ctx.fillRect(x + 14, y + 38, 4, 2);
            // 右腳直立蹬地
            ctx.fillRect(x + 24, y + 32, 4, 13);
            ctx.fillRect(x + 24, y + 45, 6, 2);
          }
        }
        ctx.restore();
      }
    };

    // 障礙物與場景物件
    let obstacles = [];
    let clouds = [
      { x: 100, y: 35, s: 0.3 },
      { x: 350, y: 55, s: 0.5 },
      { x: 620, y: 28, s: 0.4 }
    ];
    let groundOffset = 0;
    let lastMilestone = 0;

    // 🌵 Google Chrome 官方 1:1 小仙人掌 (17 x 35) - 左高右低經典不對稱分支
    function drawSmallCactus(ctx, x, y) {
      ctx.save();
      ctx.fillStyle = '#acacac';
      // 主幹
      ctx.fillRect(x + 5, y + 0, 6, 35);
      ctx.fillRect(x + 6, y + 0, 4, 1);
      // 左側高分支
      ctx.fillRect(x + 0, y + 8, 5, 4);
      ctx.fillRect(x + 0, y + 4, 4, 12);
      // 右側低分支
      ctx.fillRect(x + 11, y + 14, 5, 4);
      ctx.fillRect(x + 13, y + 10, 4, 12);
      ctx.restore();
    }

    // 🌵 Google Chrome 官方 1:1 大仙人掌 (25 x 50)
    function drawLargeCactus(ctx, x, y) {
      ctx.save();
      ctx.fillStyle = '#acacac';
      // 主幹
      ctx.fillRect(x + 8, y + 0, 9, 50);
      // 左側分支
      ctx.fillRect(x + 0, y + 14, 8, 5);
      ctx.fillRect(x + 0, y + 8, 5, 18);
      // 右側分支
      ctx.fillRect(x + 17, y + 22, 8, 5);
      ctx.fillRect(x + 20, y + 14, 5, 18);
      ctx.restore();
    }

    // 🦅 Google Chrome 官方 1:1 翼龍 (40 x 30) - 雙翅膀扇動
    function drawPterodactyl(ctx, x, y, wingUp) {
      ctx.save();
      ctx.fillStyle = '#acacac';
      // 身體與嘴喙
      ctx.fillRect(x + 12, y + 10, 18, 8);
      ctx.fillRect(x + 0, y + 12, 12, 4); // 長長尖嘴
      // 眼睛
      ctx.fillStyle = '#202124';
      ctx.fillRect(x + 10, y + 11, 2, 2);
      ctx.fillStyle = '#acacac';
      // 尾翼
      ctx.fillRect(x + 30, y + 12, 8, 4);

      // 翅膀拍動
      if (wingUp) {
        ctx.fillRect(x + 14, y - 8, 8, 18);
        ctx.fillRect(x + 18, y - 16, 5, 9);
      } else {
        ctx.fillRect(x + 14, y + 16, 8, 16);
        ctx.fillRect(x + 18, y + 24, 5, 9);
      }
      ctx.restore();
    }

    function initDinoGame() {
      if (dinoLoopId) cancelAnimationFrame(dinoLoopId);
      isDinoRunning = false;
      isDinoOver = false;
      dinoScore = 0;
      dinoSpeed = 3.6;          // 起跑慢速
      lastMilestone = 0;
      obstacles = [];
      dino.y = GROUND_Y - 47;
      dino.vy = 0;
      dino.isGrounded = true;

      updateDinoScoreBoard();
      drawDinoScene();

      if (dinoCtx) {
        dinoCtx.save();
        dinoCtx.fillStyle = '#acacac';
        dinoCtx.font = 'bold 18px "Share Tech Mono", monospace';
        dinoCtx.textAlign = 'center';
        dinoCtx.fillText('拍擊實體大按鈕 或 點擊螢幕 開始起跑！', 400, 110);
        dinoCtx.restore();
      }

      bottomHint.className = 'bottom-hint';
      bottomHint.innerHTML = '<span>🦖 拍擊實體大按鈕起跑跳躍！</span>';
    }

    function startDinoGame() {
      isDinoRunning = true;
      isDinoOver = false;
      dinoScore = 0;
      dinoSpeed = 3.6;
      lastMilestone = 0;
      obstacles = [];
      dino.y = GROUND_Y - 47;
      dino.vy = 0;
      dino.isGrounded = true;

      bottomHint.className = 'bottom-hint';
      bottomHint.innerHTML = '<span>⚡ 慢速起跑奔馳中！拍擊按鈕跳躍！</span>';
      playTone(650, 'square', 0.05, 0.35);

      if (dinoLoopId) cancelAnimationFrame(dinoLoopId);
      dinoLoop();
    }

    function dinoJump() {
      if (currentGameMode !== 3) return;
      if (isDinoOver || !isDinoRunning) {
        startDinoGame();
        return;
      }
      if (dino.isGrounded) {
        dino.vy = dino.jumpForce;
        dino.isGrounded = false;
        playTone(600, 'square', 0.05, 0.3);
      }
    }

    function updateDinoScoreBoard() {
      const padHi = String(dinoHiScore).padStart(5, '0');
      const padCur = String(Math.floor(dinoScore)).padStart(5, '0');
      if (dinoHighScoreEl) dinoHighScoreEl.innerText = padHi;
      if (dinoCurrentScoreEl) dinoCurrentScoreEl.innerText = padCur;
    }

    function spawnObstacle() {
      // 根據分數由易到難動態解鎖障礙物組合
      if (dinoScore < 150) {
        // 第一階段 (0~150分)：超友善練習期，只有單棵小仙人掌
        obstacles.push({ type: 'c_small', x: 820, y: GROUND_Y - 35, w: 17, h: 35 });
      } else if (dinoScore < 350) {
        // 第二階段 (150~350分)：小仙人掌與大仙人掌交替
        if (Math.random() < 0.6) {
          obstacles.push({ type: 'c_small', x: 820, y: GROUND_Y - 35, w: 17, h: 35 });
        } else {
          obstacles.push({ type: 'c_big', x: 820, y: GROUND_Y - 50, w: 25, h: 50 });
        }
      } else if (dinoScore < 600) {
        // 第三階段 (350~600分)：加入雙連小仙人掌與低飛翼龍
        const r = Math.random();
        if (r < 0.25) {
          obstacles.push({ type: 'ptero', x: 820, y: 138, w: 38, h: 30, wingFrame: 0 });
        } else if (r < 0.55) {
          obstacles.push({ type: 'c_double', x: 820, y: GROUND_Y - 35, w: 34, h: 35 });
        } else if (r < 0.8) {
          obstacles.push({ type: 'c_big', x: 820, y: GROUND_Y - 50, w: 25, h: 50 });
        } else {
          obstacles.push({ type: 'c_small', x: 820, y: GROUND_Y - 35, w: 17, h: 35 });
        }
      } else {
        // 第四階段 (600分以上)：大師級挑戰，翼龍高低飛與各種組合
        const r = Math.random();
        if (r < 0.3) {
          const isLow = Math.random() < 0.65;
          obstacles.push({ type: 'ptero', x: 820, y: isLow ? 138 : 105, w: 38, h: 30, wingFrame: 0 });
        } else if (r < 0.6) {
          obstacles.push({ type: 'c_double', x: 820, y: GROUND_Y - 35, w: 34, h: 35 });
        } else if (r < 0.85) {
          obstacles.push({ type: 'c_big', x: 820, y: GROUND_Y - 50, w: 25, h: 50 });
        } else {
          obstacles.push({ type: 'c_small', x: 820, y: GROUND_Y - 35, w: 17, h: 35 });
        }
      }
    }

    function drawDinoScene() {
      if (!dinoCtx) return;
      // Google 原版暗黑風格背景
      dinoCtx.fillStyle = '#202124';
      dinoCtx.fillRect(0, 0, 800, 240);

      // Google 經典像素白雲
      dinoCtx.save();
      dinoCtx.fillStyle = '#3c4043';
      clouds.forEach(c => {
        dinoCtx.fillRect(c.x, c.y + 6, 46, 12);
        dinoCtx.fillRect(c.x + 8, c.y, 24, 6);
        dinoCtx.fillRect(c.x + 16, c.y - 4, 12, 4);
      });
      dinoCtx.restore();

      // 地面主軸線（Google 灰色 #535353）
      dinoCtx.save();
      dinoCtx.strokeStyle = '#535353';
      dinoCtx.lineWidth = 1.5;
      dinoCtx.beginPath();
      dinoCtx.moveTo(0, GROUND_Y);
      dinoCtx.lineTo(800, GROUND_Y);
      dinoCtx.stroke();

      // 經典路面細碎地平線顆粒與斑紋
      dinoCtx.fillStyle = '#535353';
      for (let i = 0; i < 28; i++) {
        const gx = ((i * 32 - groundOffset) % 800 + 800) % 800;
        if (i % 3 === 0) {
          dinoCtx.fillRect(gx, GROUND_Y + 4, 8, 2);
        } else if (i % 2 === 0) {
          dinoCtx.fillRect(gx, GROUND_Y + 8, 4, 2);
          dinoCtx.fillRect(gx + 12, GROUND_Y + 12, 2, 2);
        } else {
          dinoCtx.fillRect(gx, GROUND_Y + 5, 2, 2);
        }
      }
      dinoCtx.restore();

      // 繪製原汁原味 Google 障礙物
      obstacles.forEach(obs => {
        if (obs.type === 'ptero') {
          obs.wingFrame = (obs.wingFrame + 1) % 16;
          const wingUp = obs.wingFrame < 8;
          drawPterodactyl(dinoCtx, obs.x, obs.y, wingUp);
        } else if (obs.type === 'c_small') {
          drawSmallCactus(dinoCtx, obs.x, obs.y);
        } else if (obs.type === 'c_double') {
          drawSmallCactus(dinoCtx, obs.x, obs.y);
          drawSmallCactus(dinoCtx, obs.x + 17, obs.y);
        } else if (obs.type === 'c_big') {
          drawLargeCactus(dinoCtx, obs.x, obs.y);
        }
      });

      // 繪製 Google 1:1 像素小恐龍本體
      dino.draw(dinoCtx);
    }

    function checkDinoCollision(d, obs) {
      // 貼心碰撞箱優化（避免邊角空氣牆誤判，提升手感）
      const padX = 6;
      const padY = 4;
      return (
        d.x + padX < obs.x + obs.w - 3 &&
        d.x + d.w - padX > obs.x + 3 &&
        d.y + padY < obs.y + obs.h &&
        d.y + d.h - 3 > obs.y
      );
    }

    function endDinoGame() {
      isDinoOver = true;
      isDinoRunning = false;
      playTone(150, 'sawtooth', 0.25, 0.5);
      setTimeout(() => playTone(90, 'sawtooth', 0.35, 0.6), 80);
      document.body.classList.add('screen-shake');
      setTimeout(() => document.body.classList.remove('screen-shake'), 350);

      const finalScore = Math.floor(dinoScore);
      if (finalScore > dinoHiScore) {
        dinoHiScore = finalScore;
        localStorage.setItem('dino10s_hi', dinoHiScore.toString());
      }
      updateDinoScoreBoard();

      drawDinoScene();
      if (dinoCtx) {
        dinoCtx.save();
        dinoCtx.fillStyle = 'rgba(32, 33, 36, 0.88)';
        dinoCtx.fillRect(180, 50, 440, 100);
        dinoCtx.strokeStyle = '#535353';
        dinoCtx.lineWidth = 2;
        dinoCtx.strokeRect(180, 50, 440, 100);

        dinoCtx.fillStyle = '#acacac';
        dinoCtx.font = 'bold 26px "Orbitron", monospace';
        dinoCtx.textAlign = 'center';
        dinoCtx.fillText('G A M E   O V E R', 400, 92);

        dinoCtx.fillStyle = '#00f2fe';
        dinoCtx.font = 'bold 16px "Chakra Petch", sans-serif';
        dinoCtx.fillText(`最終得分: ${finalScore} 分  ·  拍擊實體按鈕重新挑戰`, 400, 126);
        dinoCtx.restore();
      }

      let rank = '出師未捷';
      if (finalScore >= 2000) rank = '史前霸主';
      else if (finalScore >= 1200) rank = '跑酷大師';
      else if (finalScore >= 600)  rank = '迅猛龍神';
      else if (finalScore >= 300)  rank = '健步如飛';
      else if (finalScore >= 100)  rank = '新手破殼';

      addRecord(3, finalScore, rank, rank);
      bottomHint.className = 'bottom-hint wait-reset';
      bottomHint.innerHTML = `<span>💥 GAME OVER (${finalScore} 分)！拍擊實體按鈕重新起跑！</span>`;
    }

    function dinoLoop() {
      if (!isDinoRunning || currentGameMode !== 3) return;

      // 物理跳躍與重力模擬
      dino.y += dino.vy;
      dino.vy += dino.gravity;
      if (dino.y >= GROUND_Y - 47) {
        dino.y = GROUND_Y - 47;
        dino.vy = 0;
        dino.isGrounded = true;
      }
      dino.legFrame = (dino.legFrame + 1) % 8;

      // 難度梯度演算法：初始速度 3.6 (極好上手反應)，每跑180分平滑微幅加速，封頂 8.8
      dinoSpeed = 3.6 + Math.min(dinoScore / 180, 5.2);
      dinoScore += (dinoSpeed / 3.6) * 0.16;
      groundOffset = (groundOffset + dinoSpeed) % 800;

      // 百分百音效彩蛋
      const curHundred = Math.floor(dinoScore / 100);
      if (curHundred > lastMilestone) {
        lastMilestone = curHundred;
        playTone(987, 'square', 0.08, 0.4);
        setTimeout(() => playTone(1318, 'square', 0.1, 0.45), 90);
      }
      updateDinoScoreBoard();

      // 雲朵緩速漂移
      clouds.forEach(c => {
        c.x -= c.s * 0.9;
        if (c.x < -60) c.x = 860;
      });

      // 障礙物位移與碰撞偵測
      for (let i = obstacles.length - 1; i >= 0; i--) {
        const obs = obstacles[i];
        obs.x -= dinoSpeed;

        if (checkDinoCollision(dino, obs)) {
          endDinoGame();
          return;
        }

        if (obs.x + obs.w < -30) {
          obstacles.splice(i, 1);
        }
      }

      // 障礙物生成間隔：分數低時距離超寬 (480~680px)，給予充分跳躍反應時間；高速時逐步縮短
      const minGap = Math.max(300, 500 - (dinoScore * 0.28));
      const lastObs = obstacles[obstacles.length - 1];
      if (!lastObs || (800 - lastObs.x >= minGap + Math.random() * 200)) {
        spawnObstacle();
      }

      drawDinoScene();
      dinoLoopId = requestAnimationFrame(dinoLoop);
    }

    // =========================================================================
    // 🏎️ 遊戲 5：懸崖勒馬極限煞車 (Cliff Brake Challenge Engine)
    // =========================================================================
    const cliffCanvas = document.getElementById('cliffCanvas');
    const cliffCtx = cliffCanvas ? cliffCanvas.getContext('2d') : null;
    const cliffSpeedDisplay = document.getElementById('cliffSpeedDisplay');
    const cliffStatusBanner = document.getElementById('cliffStatusBanner');
    const cliffBestDisplay = document.getElementById('cliffBestDisplay');

    let cliffLoopId = null;
    let cliffState = 'READY'; // READY, ACCEL, BRAKING, STOPPED, FALLING
    const CLIFF_ROAD_Y = 195;
    const CLIFF_EDGE_X = 660; // 懸崖邊緣位置
    const CAR_START_X = 40;   // 起跑線

    let cliffBestCm = parseFloat(localStorage.getItem('cliff10s_best') || '999');

    const car = {
      x: CAR_START_X,
      y: CLIFF_ROAD_Y - 24,
      w: 60,
      h: 24,
      speed: 0,
      accel: 0.085,   // 平滑加速曲線，衝刺時間延長至 2.2~2.6 秒
      maxSpeed: 7.5,  // 調降極速，讓 1 幀僅位移 2cm，真正解鎖 0.1~2.0cm 的極限技巧！
      angle: 0,
      vy: 0,
      wheelRot: 0,
      teeter: 0,
      finalDistCm: 0,
      finalRank: '',
      isBusted: false
    };

    let cliffSkidMarks = [];
    let cliffSmokes = [];
    let cliffExhaust = [];
    let cliffExplosions = [];
    let cliffRoadOffset = 0;

    function updateCliffBestDisplay() {
      if (cliffBestDisplay) {
        cliffBestDisplay.innerText = cliffBestCm < 990 ? `${cliffBestCm} cm` : '--';
      }
    }

    function initCliffGame() {
      if (cliffLoopId) cancelAnimationFrame(cliffLoopId);
      cliffState = 'READY';
      car.x = CAR_START_X;
      car.y = CLIFF_ROAD_Y - 24;
      car.speed = 0;
      car.angle = 0;
      car.vy = 0;
      car.wheelRot = 0;
      car.teeter = 0;
      car.finalDistCm = 0;
      car.finalRank = '';
      car.isBusted = false;

      cliffSkidMarks = [];
      cliffSmokes = [];
      cliffExhaust = [];
      cliffExplosions = [];
      cliffRoadOffset = 0;

      if (cliffSpeedDisplay) cliffSpeedDisplay.innerText = '0';
      if (cliffStatusBanner) {
        cliffStatusBanner.innerHTML = '第 1 拍起跑狂飆 · 第 2 拍死命急煞！';
        cliffStatusBanner.style.color = 'var(--text)';
      }
      updateCliffBestDisplay();

      bottomHint.className = 'bottom-hint';
      bottomHint.innerHTML = '<span>🏎️ 拍擊大按鈕油門一拜狂飆！</span>';

      drawCliffScene();
    }

    function startCliffGame() {
      if (currentGameMode !== 4) return;
      if (cliffState === 'ACCEL' || cliffState === 'BRAKING') return;

      car.x = CAR_START_X;
      car.y = CLIFF_ROAD_Y - 24;
      car.speed = 0.8;
      car.angle = 0;
      car.vy = 0;
      car.teeter = 0;
      car.isBusted = false;
      cliffSkidMarks = [];
      cliffSmokes = [];
      cliffExhaust = [];
      cliffExplosions = [];

      cliffState = 'ACCEL';

      if (cliffStatusBanner) {
        cliffStatusBanner.innerHTML = '<span style="color:var(--gold);">⚡ 油門全開狂飆中！看準距離拍擊急煞！</span>';
      }
      bottomHint.className = 'bottom-hint';
      bottomHint.innerHTML = '<span>⚠️ 萬丈懸崖逼近！快看準時機死命踩煞車！！</span>';

      playTone(320, 'sawtooth', 0.15, 0.4);
      setTimeout(() => playTone(540, 'sawtooth', 0.2, 0.4), 100);

      if (cliffLoopId) cancelAnimationFrame(cliffLoopId);
      cliffLoop();
    }

    function brakeCliffGame() {
      if (currentGameMode !== 4) return;
      if (cliffState !== 'ACCEL') return;

      cliffState = 'BRAKING';

      if (cliffStatusBanner) {
        cliffStatusBanner.innerHTML = '<span style="color:var(--cyan);">💨 緊急死踩煞車中！！輪胎尖叫滑行！！</span>';
      }
      bottomHint.className = 'bottom-hint';
      bottomHint.innerHTML = '<span>💨 煞車死咬！祈禱在懸崖前停住！</span>';

      // 尖銳刺耳輪胎摩擦音效
      playTone(1800, 'square', 0.12, 0.5);
      setTimeout(() => playTone(1200, 'sawtooth', 0.25, 0.6), 60);
      document.body.classList.add('screen-shake');
      setTimeout(() => document.body.classList.remove('screen-shake'), 200);
    }

    function handleCliffTap() {
      if (currentGameMode !== 4) return;
      if (cliffState === 'READY' || cliffState === 'STOPPED') {
        startCliffGame();
      } else if (cliffState === 'ACCEL') {
        brakeCliffGame();
      }
    }

    function cliffTimeout() {
      if (cliffState === 'ACCEL') {
        // 超時未踩煞車，車輛直接失控飛下懸崖
        cliffState = 'FALLING';
      }
    }

    function spawnExplosion(x, y) {
      for (let i = 0; i < 40; i++) {
        const ang = Math.random() * Math.PI * 2;
        const spd = 2 + Math.random() * 8;
        cliffExplosions.push({
          x: x,
          y: y,
          vx: Math.cos(ang) * spd,
          vy: Math.sin(ang) * spd - 3,
          r: 4 + Math.random() * 8,
          color: Math.random() < 0.4 ? '#ff003c' : Math.random() < 0.7 ? '#ff7b00' : '#ffea00',
          life: 0,
          maxLife: 25 + Math.random() * 20
        });
      }
    }

    function cliffLoop() {
      if (currentGameMode !== 4) return;

      // 物理推進狀態機
      if (cliffState === 'ACCEL') {
        car.speed = Math.min(car.speed + car.accel, car.maxSpeed);
        car.x += car.speed;
        car.wheelRot += car.speed * 0.15;
        cliffRoadOffset = (cliffRoadOffset + car.speed) % 40;

        // 排氣管噴火火焰粒子
        if (Math.random() < 0.7) {
          cliffExhaust.push({
            x: car.x - 4,
            y: car.y + 16 + (Math.random() * 4 - 2),
            vx: -(car.speed * 0.6 + Math.random() * 2),
            vy: (Math.random() - 0.5) * 1.5,
            r: 3 + Math.random() * 3,
            color: Math.random() < 0.6 ? '#00f2fe' : '#ff416c',
            alpha: 1.0
          });
        }

        // 若直接狂飆過頭沒踩煞車 -> 飛出懸崖
        if (car.x + car.w >= CLIFF_EDGE_X) {
          cliffState = 'FALLING';
          car.vy = 0;
        }
      }
      else if (cliffState === 'BRAKING') {
        // 急煞物理：雙階段擬真阻力曲線（前段強抓咬死、末段極細微滑停，真正解鎖毫米級微距停駐）
        if (car.speed > 2.5) {
          car.speed = car.speed * 0.942 - 0.12; // 輪胎猛烈抓地咬死急煞
        } else {
          car.speed = car.speed * 0.915 - 0.035; // 末段極致細膩滑停，精準逼近崖邊！
        }
        car.wheelRot += car.speed * 0.08;

        if (car.speed > 0.03) {
          car.x += car.speed;
          car.angle = 0.035; // 急煞重心微傾

          // 左右雙輪胎焦黑煞車痕
          cliffSkidMarks.push({
            x: car.x + 10,
            y: CLIFF_ROAD_Y - 2,
            w: Math.max(3, car.speed * 1.2),
            alpha: 0.85
          });
          cliffSkidMarks.push({
            x: car.x + 46,
            y: CLIFF_ROAD_Y - 2,
            w: Math.max(3, car.speed * 1.2),
            alpha: 0.85
          });

          // 輪胎大量冒濃黑白煙
          for (let s = 0; s < 2; s++) {
            cliffSmokes.push({
              x: car.x + (s === 0 ? 10 : 46) + (Math.random() * 6 - 3),
              y: CLIFF_ROAD_Y - 4,
              vx: -(car.speed * 0.3 + Math.random() * 1.5),
              vy: -(Math.random() * 2 + 1),
              r: 4 + Math.random() * 6,
              alpha: 0.8
            });
          }
        } else {
          // 車輛徹底靜止！
          car.speed = 0;
          car.angle = 0;

          // 結算：安全停在崖上 vs 衝過頭掉落懸崖
          if (car.x + car.w <= CLIFF_EDGE_X) {
            // 🎉 成功煞住！
            cliffState = 'STOPPED';
            const distPx = CLIFF_EDGE_X - (car.x + car.w);
            const distCm = Math.max(0.1, parseFloat((distPx * 0.35).toFixed(1)));
            car.finalDistCm = distCm;
            car.isBusted = false;

            if (distPx <= 10) {
              car.teeter = 1; // 極限距離，車輪懸空輕微晃動！
            }

            let rank = '膽小烏龜 (CHICKEN)';
            if (distCm <= 3.0) rank = '神之腳步 (GODLIKE!)';
            else if (distCm <= 15.0) rank = '亡命車神 (MASTER)';
            else if (distCm <= 45.0) rank = '頂級老司機 (PRO)';
            else if (distCm <= 100.0) rank = '安全駕駛 (SAFE)';
            car.finalRank = rank;

            if (distCm < cliffBestCm) {
              cliffBestCm = distCm;
              localStorage.setItem('cliff10s_best', cliffBestCm.toString());
              updateCliffBestDisplay();
            }

            playTone(880, 'square', 0.1, 0.4);
            setTimeout(() => playTone(1320, 'square', 0.2, 0.5), 100);
            if (distCm <= 15.0) playFanfare();

            addRecord(4, distCm, 'SAFE', rank);

            if (cliffStatusBanner) {
              cliffStatusBanner.innerHTML = `<span style="color:var(--green);">🏆 成功勒馬！離懸崖僅 <strong>${distCm} cm</strong>！</span>`;
            }
            bottomHint.className = 'bottom-hint wait-reset';
            bottomHint.innerHTML = `<span>🏆 煞車成功 (${distCm} cm / ${rank})！拍擊按鈕重回起跑線！</span>`;
          } else {
            // 衝出懸崖！
            cliffState = 'FALLING';
          }
        }
      }
      else if (cliffState === 'FALLING') {
        // 自由落體墜崖物理
        car.vy += 0.65;
        car.y += car.vy;
        car.x += Math.max(car.speed, 3.2);
        car.angle += 0.06;

        if (car.y >= 260) {
          // 墜入深淵爆炸！
          cliffState = 'STOPPED';
          car.isBusted = true;
          car.finalDistCm = 0;
          car.finalRank = '粉身碎骨 (WASTED)';

          spawnExplosion(Math.min(760, car.x + 20), 230);
          playTone(90, 'sawtooth', 0.4, 0.7);
          setTimeout(() => playTone(60, 'sawtooth', 0.5, 0.8), 80);
          document.body.classList.add('screen-shake');
          setTimeout(() => document.body.classList.remove('screen-shake'), 450);

          addRecord(4, 0, 'BUSTED', '粉身碎骨');

          if (cliffStatusBanner) {
            cliffStatusBanner.innerHTML = '<span style="color:var(--red);">💥 WASTED！墜崖粉身碎骨！(0 分)</span>';
          }
          bottomHint.className = 'bottom-hint wait-reset';
          bottomHint.innerHTML = '<span>💥 墜崖粉身碎骨！拍擊按鈕重回起跑線！</span>';
        }
      }

      // 更新車速 HUD
      if (cliffSpeedDisplay) {
        cliffSpeedDisplay.innerText = Math.round(car.speed * 26);
      }

      // 更新煙霧與排氣粒子
      for (let i = cliffSmokes.length - 1; i >= 0; i--) {
        const p = cliffSmokes[i];
        p.x += p.vx;
        p.y += p.vy;
        p.r += 0.35;
        p.alpha -= 0.035;
        if (p.alpha <= 0) cliffSmokes.splice(i, 1);
      }
      for (let i = cliffExhaust.length - 1; i >= 0; i--) {
        const p = cliffExhaust[i];
        p.x += p.vx;
        p.y += p.vy;
        p.r *= 0.94;
        p.alpha -= 0.06;
        if (p.alpha <= 0 || p.r <= 0.5) cliffExhaust.splice(i, 1);
      }
      for (let i = cliffExplosions.length - 1; i >= 0; i--) {
        const e = cliffExplosions[i];
        e.x += e.vx;
        e.y += e.vy;
        e.vy += 0.2;
        e.life++;
        if (e.life >= e.maxLife) cliffExplosions.splice(i, 1);
      }

      drawCliffScene();
      cliffLoopId = requestAnimationFrame(cliffLoop);
    }

    function drawCliffScene() {
      if (!cliffCtx) return;

      // 1. 絕美暮色天際線與星空背景
      const skyGrad = cliffCtx.createLinearGradient(0, 0, 0, CLIFF_ROAD_Y);
      skyGrad.addColorStop(0, '#070c18');
      skyGrad.addColorStop(0.6, '#1f1338');
      skyGrad.addColorStop(1, '#6b1836');
      cliffCtx.fillStyle = skyGrad;
      cliffCtx.fillRect(0, 0, 800, 260);

      // 遠景剪影山脈
      cliffCtx.fillStyle = '#101322';
      cliffCtx.beginPath();
      cliffCtx.moveTo(0, CLIFF_ROAD_Y);
      cliffCtx.lineTo(60, 140);
      cliffCtx.lineTo(160, 165);
      cliffCtx.lineTo(280, 130);
      cliffCtx.lineTo(420, 170);
      cliffCtx.lineTo(540, 145);
      cliffCtx.lineTo(640, 175);
      cliffCtx.lineTo(640, CLIFF_ROAD_Y);
      cliffCtx.closePath();
      cliffCtx.fill();

      // 2. 右側萬丈懸崖深淵與海浪
      // 懸崖垂直岩壁
      const cliffWallGrad = cliffCtx.createLinearGradient(CLIFF_EDGE_X, 0, 800, 0);
      cliffWallGrad.addColorStop(0, '#1c1917');
      cliffWallGrad.addColorStop(0.4, '#120f0d');
      cliffWallGrad.addColorStop(1, '#080605');
      cliffCtx.fillStyle = cliffWallGrad;
      cliffCtx.fillRect(CLIFF_EDGE_X, CLIFF_ROAD_Y, 800 - CLIFF_EDGE_X, 260 - CLIFF_ROAD_Y);

      // 深淵底部怒濤海水
      const seaGrad = cliffCtx.createLinearGradient(CLIFF_EDGE_X, 220, 800, 260);
      seaGrad.addColorStop(0, '#0c2238');
      seaGrad.addColorStop(1, '#040b14');
      cliffCtx.fillStyle = seaGrad;
      cliffCtx.fillRect(CLIFF_EDGE_X + 15, 225, 800 - CLIFF_EDGE_X - 15, 35);

      // 海浪拍打白沫
      cliffCtx.fillStyle = 'rgba(165, 243, 252, 0.4)';
      const waveT = Date.now() * 0.003;
      for (let w = CLIFF_EDGE_X + 18; w < 790; w += 22) {
        cliffCtx.beginPath();
        cliffCtx.arc(w, 230 + Math.sin(waveT + w) * 3, 7, 0, Math.PI * 2);
        cliffCtx.fill();
      }

      // 3. 柏油路面 (0 到 CLIFF_EDGE_X)
      // 道路基礎與護欄
      cliffCtx.fillStyle = '#0f172a';
      cliffCtx.fillRect(0, CLIFF_ROAD_Y, CLIFF_EDGE_X, 260 - CLIFF_ROAD_Y);

      // 瀝青柏油路面
      cliffCtx.fillStyle = '#1e222d';
      cliffCtx.fillRect(0, CLIFF_ROAD_Y - 20, CLIFF_EDGE_X, 20);

      // 繪製地表煞車痕 (Skid Marks)
      cliffSkidMarks.forEach(skid => {
        cliffCtx.fillStyle = `rgba(18, 18, 20, ${skid.alpha})`;
        cliffCtx.fillRect(skid.x, skid.y, skid.w, 3);
      });

      // 路面黃色虛線 (完全固定靜態，徹底消除跳幀震動)
      cliffCtx.fillStyle = '#eab308';
      for (let x = 30; x < 550; x += 36) {
        cliffCtx.fillRect(x, CLIFF_ROAD_Y - 11, 18, 2);
      }

      // 4. 路面距離倒數標線 (100M, 50M, 20M, 10M)
      cliffCtx.save();
      cliffCtx.font = 'bold 10px "Share Tech Mono", monospace';
      cliffCtx.textAlign = 'center';

      // 100M
      cliffCtx.fillStyle = 'rgba(255, 255, 255, 0.4)';
      cliffCtx.fillRect(230, CLIFF_ROAD_Y - 18, 2, 16);
      cliffCtx.fillText('100M', 230, CLIFF_ROAD_Y - 22);

      // 50M
      cliffCtx.fillStyle = 'rgba(0, 242, 254, 0.6)';
      cliffCtx.fillRect(390, CLIFF_ROAD_Y - 18, 2, 16);
      cliffCtx.fillText('50M', 390, CLIFF_ROAD_Y - 22);

      // 20M
      cliffCtx.fillStyle = 'rgba(250, 204, 21, 0.8)';
      cliffCtx.fillRect(510, CLIFF_ROAD_Y - 18, 2, 16);
      cliffCtx.fillText('20M', 510, CLIFF_ROAD_Y - 22);
      cliffCtx.restore();

      // 5. 懸崖危險警示區 (560 到 CLIFF_EDGE_X)
      // 黃黑相間斜紋危險警示區
      cliffCtx.save();
      cliffCtx.beginPath();
      cliffCtx.rect(560, CLIFF_ROAD_Y - 20, CLIFF_EDGE_X - 560, 20);
      cliffCtx.clip();
      for (let hx = 540; hx < CLIFF_EDGE_X + 20; hx += 16) {
        cliffCtx.fillStyle = '#facc15';
        cliffCtx.beginPath();
        cliffCtx.moveTo(hx, CLIFF_ROAD_Y);
        cliffCtx.lineTo(hx + 8, CLIFF_ROAD_Y - 20);
        cliffCtx.lineTo(hx + 16, CLIFF_ROAD_Y - 20);
        cliffCtx.lineTo(hx + 8, CLIFF_ROAD_Y);
        cliffCtx.closePath();
        cliffCtx.fill();
      }
      cliffCtx.restore();

      // 崖邊警示立牌 (10M)
      cliffCtx.fillStyle = '#64748b';
      cliffCtx.fillRect(555, CLIFF_ROAD_Y - 36, 4, 18);
      cliffCtx.fillStyle = '#ef4444';
      cliffCtx.beginPath();
      cliffCtx.moveTo(557, CLIFF_ROAD_Y - 50);
      cliffCtx.lineTo(544, CLIFF_ROAD_Y - 32);
      cliffCtx.lineTo(570, CLIFF_ROAD_Y - 32);
      cliffCtx.closePath();
      cliffCtx.fill();
      cliffCtx.fillStyle = '#fff';
      cliffCtx.font = 'bold 9px sans-serif';
      cliffCtx.fillText('10M', 555, CLIFF_ROAD_Y - 36);

      // 崖邊暴閃警告燈
      const beaconOn = Math.floor(Date.now() / 200) % 2 === 0;
      cliffCtx.fillStyle = '#334155';
      cliffCtx.fillRect(CLIFF_EDGE_X - 6, CLIFF_ROAD_Y - 28, 6, 10);
      cliffCtx.fillStyle = beaconOn ? '#ef4444' : '#7f1d1d';
      cliffCtx.beginPath();
      cliffCtx.arc(CLIFF_EDGE_X - 3, CLIFF_ROAD_Y - 30, 4, 0, Math.PI * 2);
      cliffCtx.fill();

      // 5. 排氣火焰與輪胎煙霧
      cliffExhaust.forEach(p => {
        cliffCtx.fillStyle = p.color;
        cliffCtx.beginPath();
        cliffCtx.arc(p.x, p.y, p.r, 0, Math.PI * 2);
        cliffCtx.fill();
      });

      cliffSmokes.forEach(p => {
        cliffCtx.fillStyle = `rgba(220, 225, 230, ${p.alpha})`;
        cliffCtx.beginPath();
        cliffCtx.arc(p.x, p.y, p.r, 0, Math.PI * 2);
        cliffCtx.fill();
      });

      // 6. 繪製精緻賽車本體
      if (car.y < 250) {
        cliffCtx.save();
        const teeterAngle = (car.teeter && cliffState === 'STOPPED') ? Math.sin(Date.now() * 0.015) * 0.05 : 0;
        cliffCtx.translate(car.x + car.w / 2, car.y + car.h / 2);
        cliffCtx.rotate(car.angle + teeterAngle);

        const cx = -car.w / 2;
        const cy = -car.h / 2;

        // 車身流線跑車底色 (火焰紅)
        cliffCtx.fillStyle = '#e11d48';
        cliffCtx.beginPath();
        cliffCtx.roundRect(cx, cy + 8, car.w, car.h - 8, 4);
        cliffCtx.fill();

        // 跑車駕駛座艙頂棚
        cliffCtx.fillStyle = '#0f172a';
        cliffCtx.beginPath();
        cliffCtx.moveTo(cx + 14, cy + 8);
        cliffCtx.lineTo(cx + 22, cy + 1);
        cliffCtx.lineTo(cx + 44, cy + 1);
        cliffCtx.lineTo(cx + 50, cy + 8);
        cliffCtx.closePath();
        cliffCtx.fill();

        // 擋風玻璃反光
        cliffCtx.fillStyle = '#38bdf8';
        cliffCtx.beginPath();
        cliffCtx.moveTo(cx + 42, cy + 3);
        cliffCtx.lineTo(cx + 48, cy + 8);
        cliffCtx.lineTo(cx + 34, cy + 8);
        cliffCtx.lineTo(cx + 34, cy + 3);
        cliffCtx.closePath();
        cliffCtx.fill();

        // 尾翼
        cliffCtx.fillStyle = '#be123c';
        cliffCtx.fillRect(cx - 2, cy + 3, 6, 3);
        cliffCtx.fillRect(cx, cy + 6, 2, 4);

        // 前大燈與車前照光
        cliffCtx.fillStyle = '#fef08a';
        cliffCtx.fillRect(cx + car.w - 4, cy + 11, 4, 4);
        if (car.speed > 0) {
          cliffCtx.fillStyle = 'rgba(254, 240, 138, 0.15)';
          cliffCtx.beginPath();
          cliffCtx.moveTo(cx + car.w, cy + 13);
          cliffCtx.lineTo(cx + car.w + 90, cy + 5);
          cliffCtx.lineTo(cx + car.w + 90, cy + 22);
          cliffCtx.closePath();
          cliffCtx.fill();
        }

        // 後煞車燈 (急煞時強烈暴亮紅光)
        const isBraking = cliffState === 'BRAKING';
        cliffCtx.fillStyle = isBraking ? '#ff003c' : '#7f1d1d';
        cliffCtx.fillRect(cx - 2, cy + 11, 3, 4);
        if (isBraking) {
          cliffCtx.shadowColor = '#ff003c';
          cliffCtx.shadowBlur = 8;
          cliffCtx.fillRect(cx - 2, cy + 11, 3, 4);
          cliffCtx.shadowBlur = 0;
        }

        // 前後輪胎 (黑橡膠 + 銀色輪輻)
        const drawWheel = (wx, wy) => {
          cliffCtx.save();
          cliffCtx.translate(wx, wy);
          cliffCtx.rotate(car.wheelRot);
          cliffCtx.fillStyle = '#0f172a';
          cliffCtx.beginPath();
          cliffCtx.arc(0, 0, 7, 0, Math.PI * 2);
          cliffCtx.fill();
          cliffCtx.fillStyle = '#94a3b8';
          cliffCtx.beginPath();
          cliffCtx.arc(0, 0, 3.5, 0, Math.PI * 2);
          cliffCtx.fill();
          cliffCtx.strokeStyle = '#e2e8f0';
          cliffCtx.lineWidth = 1;
          cliffCtx.beginPath();
          cliffCtx.moveTo(-5, 0); cliffCtx.lineTo(5, 0);
          cliffCtx.moveTo(0, -5); cliffCtx.lineTo(0, 5);
          cliffCtx.stroke();
          cliffCtx.restore();
        };

        drawWheel(cx + 13, cy + car.h - 1);
        drawWheel(cx + 47, cy + car.h - 1);

        cliffCtx.restore();
      }

      // 7. 爆炸特效碎片
      cliffExplosions.forEach(e => {
        cliffCtx.fillStyle = e.color;
        cliffCtx.beginPath();
        cliffCtx.arc(e.x, e.y, e.r * (1 - e.life / e.maxLife), 0, Math.PI * 2);
        cliffCtx.fill();
      });

      // 8. 結算成績大卡牌 HUD
      if (cliffState === 'STOPPED') {
        cliffCtx.save();
        if (!car.isBusted) {
          // 🏆 煞車成功！
          cliffCtx.fillStyle = 'rgba(15, 23, 42, 0.9)';
          cliffCtx.fillRect(160, 40, 480, 110);
          cliffCtx.strokeStyle = car.finalDistCm <= 15 ? 'var(--gold)' : 'var(--cyan)';
          cliffCtx.lineWidth = 2.5;
          cliffCtx.strokeRect(160, 40, 480, 110);

          cliffCtx.fillStyle = car.finalDistCm <= 15 ? 'var(--gold)' : 'var(--cyan)';
          cliffCtx.font = 'bold 24px "Orbitron", monospace';
          cliffCtx.textAlign = 'center';
          cliffCtx.fillText(`距離崖邊: ${car.finalDistCm} cm`, 400, 80);

          cliffCtx.fillStyle = 'var(--text)';
          cliffCtx.font = 'bold 16px "Chakra Petch", sans-serif';
          cliffCtx.fillText(`榮譽稱號: ${car.finalRank}`, 400, 110);

          cliffCtx.fillStyle = 'var(--muted)';
          cliffCtx.font = '13px sans-serif';
          cliffCtx.fillText('拍擊實體大按鈕 或 點擊螢幕 重回起點！', 400, 134);
        } else {
          // 💥 墜崖粉碎！
          cliffCtx.fillStyle = 'rgba(30, 10, 15, 0.92)';
          cliffCtx.fillRect(160, 40, 480, 110);
          cliffCtx.strokeStyle = 'var(--red)';
          cliffCtx.lineWidth = 2.5;
          cliffCtx.strokeRect(160, 40, 480, 110);

          cliffCtx.fillStyle = 'var(--red)';
          cliffCtx.font = 'bold 28px "Orbitron", monospace';
          cliffCtx.textAlign = 'center';
          cliffCtx.fillText('W A S T E D', 400, 80);

          cliffCtx.fillStyle = '#fca5a5';
          cliffCtx.font = 'bold 16px "Chakra Petch", sans-serif';
          cliffCtx.fillText('💥 直墜萬丈深淵！粉身碎骨 (0 分)', 400, 110);

          cliffCtx.fillStyle = 'var(--muted)';
          cliffCtx.font = '13px sans-serif';
          cliffCtx.fillText('拍擊實體大按鈕 重新挑戰！', 400, 134);
        }
        cliffCtx.restore();
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
