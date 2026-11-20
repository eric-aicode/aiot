#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <Arduino.h>

// 前端完整 HTML/CSS/JS (內嵌於 PROGMEM，免外接 SD 卡)
const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-TW">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>電子折返跑競技計時大看板</title>
  <link href="https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@700&family=Orbitron:wght@700;900&family=Inter:wght@400;600&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg: #070b14; --card: rgba(16,26,48,0.75); --cyan: #00f2fe; --green: #00ff87; --gold: #ffb703; --red: #ff0055;
      --text: #f0f4fc; --muted: #7e8ea6;
    }
    * { margin:0; padding:0; box-sizing:border-box; user-select:none; }
    body {
      background: radial-gradient(circle at 50% 10%, #0d1a33 0%, #070b14 100%);
      color: var(--text); font-family: 'Inter', system-ui, sans-serif;
      min-height: 100vh; padding: 14px; display: flex; flex-direction: column; gap: 14px;
    }
    .header {
      display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 10px;
      background: var(--card); border: 1px solid rgba(0,242,254,0.2); border-radius: 12px; padding: 10px 18px;
    }
        .brand-wrap { display: flex; align-items: center; gap: 12px; }
    .author-logo {
      height: 44px; width: 44px; object-fit: contain; border-radius: 9px;
      background: rgba(255,255,255,0.05); padding: 2px;
      border: 1px solid rgba(0,242,254,0.3);
      filter: drop-shadow(0 0 10px rgba(0,242,254,0.35));
    }
    .author-sub { font-size: 0.72rem; color: var(--muted); letter-spacing: 0.5px; margin-top: 1px; }
    .brand { font-family: 'Chakra Petch', sans-serif; font-size: 1.25rem; font-weight:700; line-height: 1.2; }
    .brand span { color: var(--cyan); }
    .header-pills { display: flex; align-items: center; gap: 10px; flex-wrap: wrap; }
    .status-badge {
      display: inline-flex; align-items: center; gap: 6px; padding: 5px 12px;
      border-radius: 20px; font-size: 0.85rem; font-weight:600; background: rgba(0,242,254,0.15); color: var(--cyan);
    }
    .distance-badge {
      display: inline-flex; align-items: center; gap: 6px; padding: 5px 12px;
      border-radius: 20px; font-size: 0.82rem; font-weight:600; background: rgba(0,255,135,0.12); color: var(--green);
      border: 1px solid rgba(0,255,135,0.25);
    }
    .wifi-badge {
      display: inline-flex; align-items: center; gap: 6px; padding: 5px 12px;
      border-radius: 20px; font-size: 0.82rem; font-weight:600; background: rgba(0,168,255,0.15); color: #00a8ff;
      border: 1px solid rgba(0,168,255,0.3); cursor: pointer; transition: 0.2s;
    }
    .wifi-badge:hover { background: rgba(0,168,255,0.3); }
    .wifi-badge.online { background: rgba(0,255,135,0.15); color: var(--green); border-color: rgba(0,255,135,0.4); }

        /* 動態視角與比例調整 */
    .grid {
      display: grid; grid-template-columns: 1.8fr 1fr; gap: 14px; flex: 1;
      transition: grid-template-columns 0.35s cubic-bezier(0.4, 0, 0.2, 1);
    }
    .grid.mode-focus { grid-template-columns: 3.2fr 1fr; }
    .grid.mode-focus .digits { font-size: clamp(3.8rem, 10vw, 7.8rem); }
    .grid.mode-balanced { grid-template-columns: 1.8fr 1fr; }
    .grid.mode-leaderboard { grid-template-columns: 1fr 2.4fr; }
    .grid.mode-leaderboard .digits { font-size: clamp(2.4rem, 5vw, 4.2rem); }
    .grid.mode-leaderboard .tbl-wrap { max-height: 620px; }
    .grid.mode-leaderboard table { font-size: 1rem; }
    .grid.mode-leaderboard td, .grid.mode-leaderboard th { padding: 12px 10px; }

    .view-switch {
      display: inline-flex; background: rgba(0,0,0,0.45); padding: 3px; border-radius: 20px;
      border: 1px solid rgba(255,255,255,0.1); gap: 2px;
    }
    .view-btn {
      background: transparent; border: none; color: var(--muted); font-size: 0.8rem; font-weight: 600;
      padding: 5px 11px; border-radius: 16px; cursor: pointer; transition: 0.2s;
    }
    .view-btn:hover { color: #fff; }
    .view-btn.active {
      background: var(--cyan); color: #040812; font-weight: 700;
      box-shadow: 0 0 10px rgba(0,242,254,0.4);
    }
    @media(max-width:960px){ .grid { grid-template-columns: 1fr; } }
    .card { background: var(--card); border: 1px solid rgba(255,255,255,0.08); border-radius: 16px; padding: 18px; display: flex; flex-direction: column; gap: 14px; }
    
    /* 跑道與跑者 */
    .track { display: flex; align-items: center; background: rgba(0,0,0,0.3); border-radius: 10px; padding: 8px 16px; position: relative; }
    .track-line { flex:1; height: 8px; background: rgba(255,255,255,0.1); margin: 0 10px; border-radius: 4px; position: relative; }
    .track-prog { height: 100%; width: 0%; background: linear-gradient(90deg, var(--cyan), var(--green)); border-radius: 4px; transition: width 0.1s linear; }
    .runner { position: absolute; top: -20px; left: 0%; transform: translateX(-50%); font-size: 1.6rem; transition: left 0.1s linear, transform 0.2s; }
    .runner.left { transform: translateX(-50%) scaleX(-1); }

    /* 巨型計時螢幕 (鎖定等寬數字，徹底消除計時跳動震動) */
    .timer-box {
      background: radial-gradient(circle, #0a172e 0%, #040812 100%);
      border: 2px solid rgba(0,242,254,0.3); border-radius: 16px; padding: 30px 10px;
      display: flex; flex-direction: column; align-items: center; justify-content: center;
      box-shadow: inset 0 0 30px rgba(0,0,0,0.8), 0 0 20px rgba(0,242,254,0.15); position: relative;
      overflow: hidden;
    }
    .phase-tag { font-family: 'Chakra Petch', sans-serif; color: var(--cyan); letter-spacing: 2px; font-size: 0.92rem; margin-bottom: 6px; font-weight:700; }
    .digits {
      font-family: 'Consolas', 'Menlo', 'Monaco', 'Courier New', monospace;
      font-variant-numeric: tabular-nums;
      font-feature-settings: "tnum" 1, "zero" 1;
      font-size: clamp(3.2rem, 8vw, 6.2rem);
      font-weight: 900; color: #fff;
      text-shadow: 0 0 20px rgba(0,242,254,0.5);
      display: inline-flex; align-items: baseline; justify-content: flex-start;
      line-height: 1.1; white-space: nowrap;
      width: 8.8ch; text-align: left;
    }
    .digits span {
      color: var(--cyan); font-size: 0.7em;
      display: inline-block; width: 4.2ch; text-align: left;
    }
    
    .overlay-count {
      position: absolute; inset:0; background: rgba(5,10,20,0.92); display:flex; align-items:center; justify-content:center;
      font-family:'Orbitron'; font-size: 7.5rem; color: var(--gold); text-shadow: 0 0 35px var(--gold); border-radius: 14px;
      animation: countZoom 0.85s ease infinite;
    }
    @keyframes countZoom {
      0% { transform: scale(0.6); opacity: 0; }
      40% { transform: scale(1.05); opacity: 1; }
      80% { transform: scale(1); opacity: 1; }
      100% { transform: scale(1.2); opacity: 0; }
    }

    /* 參數設定列 */
    .config-bar {
      display: flex; align-items: center; justify-content: space-between; background: rgba(255,255,255,0.03);
      padding: 10px 14px; border-radius: 10px; border: 1px solid rgba(255,255,255,0.06); font-size: 0.88rem;
    }
    .config-bar select {
      background: #0f1c33; color: var(--cyan); border: 1px solid rgba(0,242,254,0.4); padding: 5px 10px; border-radius: 6px; font-weight: bold; outline:none;
    }

    /* 分段成績與即時狀態 */
    .splits { display: grid; grid-template-columns: 1fr 1fr 1.2fr; gap: 10px; }
    .split-col { background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.08); border-radius: 10px; padding: 12px; }
    .split-col.locked { border-color: var(--cyan); background: rgba(0,242,254,0.05); }
    .split-col.gold { border-color: var(--gold); background: rgba(255,183,3,0.08); }
    .split-lbl { font-size: 0.75rem; color: var(--muted); }
    .split-val { font-family: 'Consolas', 'Menlo', monospace; font-variant-numeric: tabular-nums; font-size: 1.55rem; font-weight: 700; margin-top: 4px; }
    .split-col.gold .split-val { color: var(--gold); }

    /* 按鈕 */
    .controls { display: grid; grid-template-columns: 2fr 1fr 1fr; gap: 10px; }
    .btn {
      padding: 13px; border-radius: 10px; font-weight: 700; font-size: 0.95rem; border: none; cursor: pointer;
      display: flex; align-items: center; justify-content: center; gap: 6px; transition: transform 0.15s;
    }
    .btn:active { transform: scale(0.97); }
    .btn-start { background: linear-gradient(135deg, var(--cyan), #00a8ff); color: #020712; }
    .btn-hit { background: linear-gradient(135deg, var(--red), #ff4757); color: #fff; }
    .btn-reset { background: rgba(255,255,255,0.08); color: var(--muted); }

    /* 排行榜 */
    .tbl-wrap { overflow-y: auto; max-height: 480px; }
    table { width: 100%; border-collapse: collapse; font-size: 0.88rem; text-align: left; }
    th { padding: 9px; background: rgba(0,0,0,0.3); color: var(--muted); position: sticky; top:0; }
    td { padding: 9px; border-top: 1px solid rgba(255,255,255,0.05); }
    .rank-1 { color: #ffd700; font-weight:bold; }
    .hidden { display: none !important; }

    /* Wi-Fi 模態視窗 */
    .modal-backdrop {
      position: fixed; inset: 0; background: rgba(0,0,0,0.75); display: flex; align-items: center; justify-content: center; z-index: 999;
      backdrop-filter: blur(4px);
    }
    .modal-box {
      background: #0f1c33; border: 1px solid rgba(0,242,254,0.35); border-radius: 14px; padding: 22px; width: 92%; max-width: 440px;
      display: flex; flex-direction: column; gap: 14px; box-shadow: 0 10px 40px rgba(0,0,0,0.8);
    }
    .modal-header { display: flex; justify-content: space-between; align-items: center; }
    .modal-header h3 { font-size: 1.15rem; color: var(--cyan); }
    .modal-close { background: none; border: none; color: var(--muted); font-size: 1.4rem; cursor: pointer; }
    .modal-input {
      background: #08101e; border: 1px solid rgba(255,255,255,0.15); border-radius: 8px; padding: 10px 12px; color: #fff; font-size: 0.9rem; width: 100%; outline: none;
    }
    .modal-input:focus { border-color: var(--cyan); }
    .modal-btns { display: flex; gap: 10px; justify-content: flex-end; margin-top: 6px; }
    .btn-sm { padding: 8px 14px; font-size: 0.85rem; border-radius: 8px; border: none; cursor: pointer; font-weight: bold; }
    .wifi-list { max-height: 140px; overflow-y: auto; background: rgba(0,0,0,0.3); border-radius: 6px; border: 1px solid rgba(255,255,255,0.08); }
    .wifi-item { padding: 8px 12px; display: flex; justify-content: space-between; cursor: pointer; font-size: 0.85rem; border-bottom: 1px solid rgba(255,255,255,0.04); }
    .wifi-item:hover { background: rgba(0,242,254,0.1); color: var(--cyan); }
  </style>
</head>
<body>
  <div class="header">
    <div class="brand-wrap">
      <img src="data:image/png;base64,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" class="author-logo" alt="Author Logo">
      <div>
        <div class="brand">電子折返跑 <span>無線大看板</span></div>
        <div class="author-sub">AIoT 體適能智慧運動科技</div>
      </div>
    </div>
    <div class="header-pills">
      <div class="view-switch">
        <button class="view-btn" id="btnViewFocus" onclick="setViewMode('focus')" title="專注計時：放大計時巨幕">⏱ 專注</button>
        <button class="view-btn active" id="btnViewBalanced" onclick="setViewMode('balanced')" title="標準平衡：雙欄並重">⚖️ 平衡</button>
        <button class="view-btn" id="btnViewLeaderboard" onclick="setViewMode('leaderboard')" title="大螢幕排行榜：展開全班排名">🏆 排行</button>
      </div>
      <div class="distance-badge" id="badgeDistance">📡 距離測算中...</div>
      <button class="wifi-badge" id="badgeWifi" onclick="openWifiModal()">🌐 外部 Wi-Fi 設定</button>
      <div class="status-badge" id="badgeStatus">待機就緒</div>
    </div>
  </div>

  <div class="grid mode-balanced" id="mainGrid">
    <div class="card">
      <div class="config-bar">
        <span>🏃 測驗設定</span>
        <div>
          <label style="color:var(--muted); margin-right:6px;">折返回合：</label>
          <select id="selectLaps" onchange="changeLaps()">
            <option value="2">1 回 (2 趟)</option>
            <option value="4" selected>2 回 (4 趟 · 標準)</option>
            <option value="6">3 回 (6 趟)</option>
            <option value="8">4 回 (8 趟)</option>
            <option value="10">5 回 (10 趟)</option>
            <option value="12">6 回 (12 趟)</option>
            <option value="16">8 回 (16 趟)</option>
            <option value="20">10 回 (20 趟)</option>
          </select>
        </div>
      </div>

      <div class="track">
        <span id="labelStart">🏁 起點 (A)</span>
        <div class="track-line">
          <div class="runner" id="runner">🏃</div>
          <div class="track-prog" id="prog"></div>
        </div>
        <span id="labelTurn">🚩 折返點 (B)</span>
      </div>

      <div class="timer-box">
        <div class="phase-tag" id="phaseTag">READY TO RACE</div>
        <div class="digits" id="digits">00:00<span>.000</span></div>
        <div class="overlay-count hidden" id="overlayCount">3</div>
      </div>

      <div class="splits">
        <div class="split-col" id="colLeg1">
          <div class="split-lbl" id="lblLeg1">當前趟次</div>
          <div class="split-val" id="valLeg1">第 0 趟</div>
        </div>
        <div class="split-col" id="colLeg2">
          <div class="split-lbl">上一趟分段耗時</div>
          <div class="split-val" id="valLeg2">--:--.--</div>
        </div>
        <div class="split-col gold" id="colTotal">
          <div class="split-lbl">總成績</div>
          <div class="split-val" id="valTotal">--:--.--</div>
        </div>
      </div>

      <div class="controls">
        <button class="btn btn-start" onclick="startWebCountdown()">⚡ 倒數發令起跑</button>
        <button class="btn btn-hit" onclick="sendCmd('hit')">🥊 模擬按鈕拍擊</button>
        <button class="btn btn-reset" onclick="sendCmd('reset')">🔄 歸零重置</button>
      </div>
    </div>

    <div class="card">
      <div style="display:flex; justify-content:space-between; align-items:center;">
        <h3>🏆 測驗排行榜</h3>
        <button style="padding:4px 8px; border-radius:6px; background:#222; color:#aaa; border:none; cursor:pointer;" onclick="clearHistory()">清空</button>
      </div>
      <div class="tbl-wrap">
        <table>
          <thead>
            <tr><th>名次</th><th>選手</th><th>趟數</th><th>總成績</th></tr>
          </thead>
          <tbody id="tblBody">
            <tr><td colspan="4" style="text-align:center; color:#555; padding:20px;">尚無紀錄</td></tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>

  <!-- Wi-Fi 設定彈跳視窗 -->
  <div class="modal-backdrop hidden" id="modalWifi">
    <div class="modal-box">
      <div class="modal-header">
        <h3>🌐 外部 Wi-Fi 設定 (雙 IP 模式)</h3>
        <button class="modal-close" onclick="closeWifiModal()">&times;</button>
      </div>
      <div style="font-size:0.83rem; color:var(--muted); line-height:1.4;">
        連上外部 Wi-Fi 後，全班免換熱點即可大螢幕投影！<br>
        <strong style="color:var(--cyan);">本地熱點 (192.168.4.1) 依然並存有效。</strong>
      </div>
      <div>
        <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:4px;">
          <label style="font-size:0.85rem;">Wi-Fi 名稱 (SSID)：</label>
          <button class="btn-sm" style="background:rgba(255,255,255,0.08); color:var(--cyan);" onclick="scanWifi()">🔍 掃描周圍</button>
        </div>
        <div class="wifi-list hidden" id="wifiScanList"></div>
        <input type="text" class="modal-input" id="wifiSsid" placeholder="選擇或輸入基地台名稱">
      </div>
      <div>
        <label style="font-size:0.85rem; display:block; margin-bottom:4px;">密碼 (Password)：</label>
        <input type="password" class="modal-input" id="wifiPass" placeholder="Wi-Fi 密碼 (無密碼請留空)">
      </div>
      <div class="modal-btns">
        <button class="btn-sm" style="background:#333; color:#aaa;" onclick="clearWifi()">清除連線 (純離線)</button>
        <button class="btn-sm" style="background:linear-gradient(135deg, var(--cyan), #00a8ff); color:#000;" onclick="saveWifi()">連線並儲存</button>
      </div>
    </div>
  </div>

  <script>
    let state = 'IDLE';
    let startT = 0;
    let animId = null;
    let totalLaps = 4; // 預設共 4 趟 (2 回合標準)
    let records = JSON.parse(localStorage.getItem('shuttle_ap_records') || '[]');

    // Web Audio 合成音效
    let audioCtx = null;
    const playBeep = (freq, dur = 0.08) => {
      try {
        if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        if (audioCtx.state === 'suspended') audioCtx.resume();
        const osc = audioCtx.createOscillator();
        const gain = audioCtx.createGain();
        osc.frequency.setValueAtTime(freq, audioCtx.currentTime);
        gain.gain.setValueAtTime(0.25, audioCtx.currentTime);
        gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + dur);
        osc.connect(gain); gain.connect(audioCtx.destination);
        osc.start(); osc.stop(audioCtx.currentTime + dur);
      } catch(e){}
    };

    const fmt = (ms) => {
      let s = Math.floor(ms/1000), f = Math.floor(ms%1000);
      let m = Math.floor(s/60); s = s % 60;
      return `${String(m).padStart(2,'0')}:${String(s).padStart(2,'0')}.${String(f).padStart(3,'0')}`;
    };

    const timerLoop = () => {
      if (state.startsWith('RUNNING')) {
        let el = performance.now() - startT;
        let s = Math.floor(el/1000), f = Math.floor(el%1000), m = Math.floor(s/60); s = s % 60;
        document.getElementById('digits').innerHTML = `${String(m).padStart(2,'0')}:${String(s).padStart(2,'0')}<span>.${String(f).padStart(3,'0')}</span>`;

        animId = requestAnimationFrame(timerLoop);
      }
    };

    const sendCmd = (act, extra = '') => {
      fetch('/cmd?action=' + act + extra).catch(e => console.warn(e));
    };

    const changeLaps = () => {
      totalLaps = parseInt(document.getElementById('selectLaps').value);
      sendCmd('set_laps', '&laps=' + totalLaps);
    };

    // 前端視覺震撼 3-2-1 倒數
    const startWebCountdown = () => {
      if (state !== 'IDLE' && state !== 'FINISHED') return;
      state = 'COUNTDOWN';
      const overlay = document.getElementById('overlayCount');
      overlay.classList.remove('hidden');

      let count = 3;
      overlay.textContent = count;
      playBeep(1500, 0.08);

      const timer = setInterval(() => {
        count--;
        if (count > 0) {
          overlay.textContent = count;
          playBeep(1500, 0.08);
        } else {
          clearInterval(timer);
          overlay.classList.add('hidden');
          playBeep(2600, 0.35); // GO 發令音！
          sendCmd('start'); // 通知硬體鳴槍起跑！
        }
      }, 1000);
    };

    const renderTbl = () => {
      if (!records.length) {
        document.getElementById('tblBody').innerHTML = '<tr><td colspan="4" style="text-align:center; color:#555; padding:20px;">尚無紀錄</td></tr>';
        return;
      }
      records.sort((a,b) => a.tot - b.tot);
      document.getElementById('tblBody').innerHTML = records.map((r,i) => `
        <tr>
          <td class="${i===0?'rank-1':''}">${i===0?'🥇':i===1?'🥈':i===2?'🥉':i+1}</td>
          <td><strong>${r.name}</strong></td>
          <td>${r.laps} 趟</td>
          <td style="color:var(--cyan); font-weight:bold;">${(r.tot/1000).toFixed(2)}s</td>
        </tr>
      `).join('');
    };

    // 動態視角與比例調整
    const setViewMode = (mode) => {
      const g = document.getElementById('mainGrid');
      g.classList.remove('mode-focus', 'mode-balanced', 'mode-leaderboard');
      g.classList.add('mode-' + mode);

      document.querySelectorAll('.view-btn').forEach(b => b.classList.remove('active'));
      if (mode === 'focus') document.getElementById('btnViewFocus').classList.add('active');
      else if (mode === 'leaderboard') document.getElementById('btnViewLeaderboard').classList.add('active');
      else document.getElementById('btnViewBalanced').classList.add('active');

      localStorage.setItem('shuttle_view_mode', mode);
    };

    const clearHistory = () => {
      if(confirm('清空排行榜？')){ records = []; localStorage.removeItem('shuttle_ap_records'); renderTbl(); }
    };

    // Wi-Fi 互動
    const openWifiModal = () => document.getElementById('modalWifi').classList.remove('hidden');
    const closeWifiModal = () => document.getElementById('modalWifi').classList.add('hidden');

    const scanWifi = () => {
      const listEl = document.getElementById('wifiScanList');
      listEl.innerHTML = '<div style="padding:10px; color:#aaa; font-size:0.82rem;">📡 掃描周圍 Wi-Fi 中...</div>';
      listEl.classList.remove('hidden');
      fetch('/scan_wifi')
        .then(r => r.json())
        .then(arr => {
          if (!arr.length) { listEl.innerHTML = '<div style="padding:10px; color:#666;">未發現 Wi-Fi</div>'; return; }
          // 依 RSSI 訊號強弱降序排序 (訊號好排在前)
          arr.sort((a,b) => b.rssi - a.rssi);
          listEl.innerHTML = arr.map(ap => `
            <div class="wifi-item" onclick="selectWifi('${ap.ssid}')">
              <span>📶 <strong>${ap.ssid}</strong> ${ap.ch ? '<small style="color:#666;">(CH'+ap.ch+')</small>' : ''}</span>
              <span style="color:${ap.rssi > -65 ? 'var(--green)' : ap.rssi > -78 ? 'var(--gold)' : 'var(--muted)'}; font-weight:600;">${ap.rssi} dBm</span>
            </div>
          `).join('');
        })
        .catch(() => { listEl.innerHTML = '<div style="padding:10px; color:#f55;">掃描失敗</div>'; });
    };

    const selectWifi = (ssid) => {
      document.getElementById('wifiSsid').value = ssid;
      document.getElementById('wifiScanList').classList.add('hidden');
    };

    const saveWifi = () => {
      const ssid = document.getElementById('wifiSsid').value.trim();
      const pass = document.getElementById('wifiPass').value;
      if (!ssid) { alert('請輸入 SSID'); return; }
      fetch(`/set_wifi?ssid=${encodeURIComponent(ssid)}&pass=${encodeURIComponent(pass)}`)
        .then(() => {
          alert('Wi-Fi 設定已儲存！主控台正在連線中...');
          closeWifiModal();
          setTimeout(fetchWifiStatus, 4000);
        });
    };

    const clearWifi = () => {
      if (confirm('清除外部 Wi-Fi 設定並回歸純離線模式？')) {
        fetch('/clear_wifi').then(() => {
          alert('已清除，回歸純熱點模式！');
          closeWifiModal();
          setTimeout(fetchWifiStatus, 1000);
        });
      }
    };

    const fetchWifiStatus = () => {
      fetch('/wifi_status')
        .then(r => r.json())
        .then(d => updateWifiBadge(d))
        .catch(e => console.warn(e));
    };

    const updateWifiBadge = (d) => {
      const btn = document.getElementById('badgeWifi');
      if (d.connected && d.ip) {
        btn.classList.add('online');
        btn.textContent = `🌐 外網 IP: ${d.ip} (${d.ssid})`;
        btn.title = `主控台已連入 ${d.ssid} (頻道: ${d.channel})`;
      } else {
        btn.classList.remove('online');
        btn.textContent = '🌐 外部 Wi-Fi: 未連線';
        btn.title = '點擊設定連線校園 Wi-Fi';
      }
    };

    // 建立 Server-Sent Events (SSE) 即時監聽
    const initSSE = () => {
      const evt = new EventSource('/events');

      evt.addEventListener('start', e => {
        let d = JSON.parse(e.data);
        state = 'RUNNING';
        startT = performance.now();
        document.getElementById('overlayCount').classList.add('hidden');
        document.getElementById('phaseTag').textContent = `🏃 第 1 趟 / 共 ${totalLaps} 趟 (衝向折返點 B)`;
        document.getElementById('valLeg1').textContent = `第 1 趟`;
        document.getElementById('badgeStatus').textContent = '去程衝刺中';
        document.getElementById('runner').style.left = '92%';
        document.getElementById('prog').style.width = '92%';
        document.getElementById('runner').classList.remove('left');
        cancelAnimationFrame(animId);
        animId = requestAnimationFrame(timerLoop);
      });

      evt.addEventListener('lap', e => {
        let d = JSON.parse(e.data);
        document.getElementById('valLeg1').textContent = `第 ${d.currentLeg} 趟`;
        document.getElementById('valLeg2').textContent = (d.legMs/1000).toFixed(2) + 's';
        playBeep(2200, 0.1);

        let isOdd = (d.currentLeg % 2 === 1);
        document.getElementById('phaseTag').textContent = `🏃 第 ${d.currentLeg} 趟 / 共 ${d.targetLegs} 趟 (${isOdd ? '衝向折返點 B' : '衝向起點 A'})`;
        document.getElementById('badgeStatus').textContent = isOdd ? '衝向折返點' : '衝回起點';

        if (isOdd) {
          document.getElementById('runner').style.left = '92%';
          document.getElementById('prog').style.width = '92%';
          document.getElementById('runner').classList.remove('left');
        } else {
          document.getElementById('runner').style.left = '0%';
          document.getElementById('prog').style.width = '0%';
          document.getElementById('runner').classList.add('left');
        }
      });

      evt.addEventListener('finish', e => {
        let d = JSON.parse(e.data);
        state = 'FINISHED';
        cancelAnimationFrame(animId);
        document.getElementById('valLeg2').textContent = (d.lastLegMs/1000).toFixed(2) + 's';
        document.getElementById('valTotal').textContent = (d.totalMs/1000).toFixed(2) + 's';
        document.getElementById('colTotal').classList.add('gold');
        document.getElementById('phaseTag').textContent = 'FINISHED (完賽)';
        document.getElementById('badgeStatus').textContent = '完賽結算！';
        playBeep(1800, 0.4);

        records.push({ name: `選手 #${String(records.length+1).padStart(2,'0')}`, laps: totalLaps, tot: d.totalMs });
        localStorage.setItem('shuttle_ap_records', JSON.stringify(records));
        renderTbl();
      });

      evt.addEventListener('reset', e => {
        state = 'IDLE';
        cancelAnimationFrame(animId);
        document.getElementById('digits').innerHTML = '00:00<span>.000</span>';
        document.getElementById('colTotal').classList.remove('gold');
        document.getElementById('valLeg1').textContent = '第 0 趟';
        document.getElementById('valLeg2').textContent = '--:--.--';
        document.getElementById('valTotal').textContent = '--:--.--';
        document.getElementById('runner').style.left = '0%';
        document.getElementById('prog').style.width = '0%';
        document.getElementById('runner').classList.remove('left');
        document.getElementById('overlayCount').classList.add('hidden');
        document.getElementById('phaseTag').textContent = 'READY TO RACE';
        document.getElementById('badgeStatus').textContent = '待機就緒';
      });

      evt.addEventListener('telemetry', e => {
        let d = JSON.parse(e.data);
        if (d.distance) {
          document.getElementById('badgeDistance').textContent = `📡 估算距離：${d.distance} (${d.rssi} dBm)`;
        }
        if (d.externalIp !== undefined) {
          updateWifiBadge({ connected: !!d.externalIp, ip: d.externalIp, ssid: d.wifiSsid, channel: d.channel });
        }
      });
    };

    window.onload = () => {
      const savedMode = localStorage.getItem('shuttle_view_mode') || 'balanced';
      setViewMode(savedMode);
      changeLaps();
      renderTbl();
      initSSE();
      fetchWifiStatus();
    };
  </script>
</body>
</html>
)rawliteral";

#endif
