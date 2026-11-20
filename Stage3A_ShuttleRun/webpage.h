#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <Arduino.h>

const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-TW">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ArenaPod ⚡ 雙板直線折返跑 (Stage 3A)</title>
  <link href="https://fonts.googleapis.com/css2?family=Orbitron:wght@700;900&family=Noto+Sans+TC:wght@500;700;900&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg: #0f172a;
      --card: #1e293b;
      --primary: #3b82f6;
      --success: #10b981;
      --accent: #f59e0b;
      --danger: #ef4444;
      --text: #f8fafc;
      --muted: #94a3b8;
    }
    * { margin:0; padding:0; box-sizing:border-box; }
    body {
      background: var(--bg);
      color: var(--text);
      font-family: 'Noto Sans TC', sans-serif;
      min-height: 100vh;
      padding: 1rem;
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 1.2rem;
    }
    .container {
      width: 100%;
      max-width: 480px;
      display: flex;
      flex-direction: column;
      gap: 1rem;
    }
    .header {
      background: var(--card);
      border: 1px solid #334155;
      border-radius: 1rem;
      padding: 1.1rem 1.3rem;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .title { font-size: 1.15rem; font-weight: 900; color: #60a5fa; }
    .status-badge {
      font-size: 0.8rem;
      font-weight: 700;
      padding: 0.25rem 0.65rem;
      border-radius: 9999px;
      background: rgba(59, 130, 246, 0.2);
      color: #93c5fd;
      border: 1px solid rgba(59, 130, 246, 0.3);
    }
    .role-banner {
      background: #1e293b;
      border: 1px solid #3b82f6;
      border-radius: 0.85rem;
      padding: 0.75rem 1rem;
      text-align: center;
      font-weight: 700;
      font-size: 0.95rem;
    }
    .lap-selector {
      background: var(--card);
      border: 1px solid #334155;
      border-radius: 0.85rem;
      padding: 0.75rem 1rem;
      display: flex;
      flex-direction: column;
      gap: 0.5rem;
    }
    .lap-selector-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      font-size: 0.85rem;
      font-weight: 700;
      color: var(--muted);
    }
    .lap-chips {
      display: flex;
      gap: 0.5rem;
    }
    .lap-chip {
      flex: 1;
      padding: 0.55rem 0.2rem;
      border-radius: 0.6rem;
      font-size: 0.82rem;
      font-weight: 800;
      background: #0f172a;
      border: 1px solid #334155;
      color: #94a3b8;
      cursor: pointer;
      text-align: center;
      transition: all 0.15s ease;
    }
    .lap-chip.active {
      background: #2563eb;
      border-color: #60a5fa;
      color: #ffffff;
      box-shadow: 0 0 10px rgba(37, 99, 235, 0.4);
    }
    .timer-card {
      background: radial-gradient(circle at 50% 20%, #1e293b 0%, #0f172a 100%);
      border: 2px solid #3b82f6;
      border-radius: 1.25rem;
      padding: 1.6rem 1.2rem;
      text-align: center;
      box-shadow: 0 10px 25px -5px rgba(59, 130, 246, 0.2);
    }
    .timer-label { font-size: 0.85rem; font-weight: 700; color: var(--muted); letter-spacing: 0.1em; text-transform: uppercase; }
    .timer-display {
      font-family: 'Orbitron', monospace;
      font-size: 3.6rem;
      font-weight: 900;
      color: #38bdf8;
      text-shadow: 0 0 20px rgba(56, 189, 248, 0.4);
      margin: 0.4rem 0;
      line-height: 1;
    }
    .race-state {
      font-size: 1.05rem;
      font-weight: 800;
      color: #facc15;
      min-height: 1.8rem;
    }
    .laps-container {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(130px, 1fr));
      gap: 0.6rem;
      max-height: 220px;
      overflow-y: auto;
    }
    .lap-box {
      background: var(--card);
      border: 1px solid #334155;
      border-radius: 0.85rem;
      padding: 0.75rem;
      text-align: center;
    }
    .lap-title { font-size: 0.75rem; color: var(--muted); font-weight: 700; }
    .lap-val {
      font-family: 'Orbitron', monospace;
      font-size: 1.4rem;
      font-weight: 900;
      color: #34d399;
      margin-top: 0.25rem;
    }
    .btn-group {
      display: flex;
      gap: 0.75rem;
    }
    .btn {
      flex: 1;
      padding: 1.1rem;
      border-radius: 0.85rem;
      font-size: 1.15rem;
      font-weight: 900;
      cursor: pointer;
      border: none;
      transition: all 0.15s ease;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 0.5rem;
    }
    .btn:active { transform: scale(0.97); }
    .btn-go {
      background: linear-gradient(135deg, #2563eb, #1d4ed8);
      color: white;
      box-shadow: 0 4px 15px rgba(37, 99, 235, 0.4);
    }
    .btn-reset {
      background: #334155;
      color: #e2e8f0;
    }
    .ota-link {
      text-align: center;
      font-size: 0.82rem;
      color: var(--muted);
      margin-top: 0.5rem;
    }
    .ota-link a { color: #60a5fa; text-decoration: none; }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <div class="title">🏃 ArenaPod 折返跑</div>
      <div class="status-badge" id="connStatus">● 連線就緒</div>
    </div>

    <div class="role-banner" id="roleBanner">
      ⏳ 讀取裝置角色中...
    </div>

    <!-- 折返趟數選擇器 (只有 Master 在 IDLE 狀態時可切換) -->
    <div class="lap-selector" id="lapSelectorBox">
      <div class="lap-selector-header">
        <span>🎯 折返跑設定</span>
        <span id="lapSummary" style="color:#60a5fa; font-weight:800;">1 次折返 (共 2 趟)</span>
      </div>
      <div class="lap-chips">
        <button class="lap-chip active" id="chip1" onclick="setLaps(1)">1次 (2趟)</button>
        <button class="lap-chip" id="chip2" onclick="setLaps(2)">2次 (4趟)</button>
        <button class="lap-chip" id="chip3" onclick="setLaps(3)">3次 (6趟)</button>
        <button class="lap-chip" id="chip4" onclick="setLaps(4)">4次 (8趟)</button>
      </div>
    </div>

    <div class="timer-card">
      <div class="timer-label">TOTAL TIME</div>
      <div class="timer-display" id="timeMain">00.00</div>
      <div class="race-state" id="raceState">起點就緒，請拍實體按鈕或按 GO 起跑</div>
    </div>

    <!-- 動態分段成績列表 -->
    <div class="laps-container" id="lapsContainer">
      <div class="lap-box">
        <div class="lap-title">第 1 次 去程 (Pod 1)</div>
        <div class="lap-val">--.--</div>
      </div>
      <div class="lap-box">
        <div class="lap-title">第 1 次 回程 (Pod 0)</div>
        <div class="lap-val">--.--</div>
      </div>
    </div>

    <div class="btn-group">
      <button class="btn btn-go" id="btnGo" onclick="sendCmd('start')">▶️ GO 起跑</button>
      <button class="btn btn-reset" onclick="sendCmd('reset')">🔄 重新就位</button>
    </div>

    <div class="ota-link">
      Wemos D1 mini 雙板通用多趟折返跑 (Stage 3A) ｜ <a href="/update">空中 OTA 韌體更新</a>
    </div>
  </div>

  <script>
    let timerInterval = null;
    let startTime = 0;
    let localRole = 'UNPAIRED';
    let currentTargetLaps = 1;

    function sendCmd(cmd) {
      fetch('/cmd?action=' + cmd).then(r => r.json()).then(d => {
        updateUI(d);
      }).catch(e => console.log(e));
    }

    function setLaps(n) {
      currentTargetLaps = n;
      for (let i = 1; i <= 4; i++) {
        const c = document.getElementById('chip' + i);
        if (c) c.classList.toggle('active', i === n);
      }
      sendCmd('setLaps&count=' + n);
    }

    function updateUI(d) {
      if (!d) return;

      const banner = document.getElementById('roleBanner');
      if (d.role === 'UNPAIRED') {
        banner.innerHTML = '⚠️ <strong>尚未配對！</strong> 請在桌上先拍一下要當起點的盒子';
        banner.style.borderColor = '#ef4444';
        banner.style.color = '#f87171';
      } else if (d.role === 'MASTER') {
        banner.innerHTML = '🟢 <strong>起點裁判大腦 (Master / Pod 0)</strong> ｜ 已鎖定搭檔';
        banner.style.borderColor = '#10b981';
        banner.style.color = '#34d399';
      } else if (d.role === 'SLAVE') {
        banner.innerHTML = '🔵 <strong>折返點子機 (Slave / Pod 1)</strong> ｜ 請連線至起點的熱點';
        banner.style.borderColor = '#3b82f6';
        banner.style.color = '#60a5fa';
      }

      // 更新趟數選擇器
      if (d.targetLaps) {
        currentTargetLaps = d.targetLaps;
        for (let i = 1; i <= 4; i++) {
          const c = document.getElementById('chip' + i);
          if (c) c.classList.toggle('active', i === d.targetLaps);
        }
        document.getElementById('lapSummary').innerText = `${d.targetLaps} 次折返 (共 ${d.totalLegs} 趟)`;
      }

      const raceStateEl = document.getElementById('raceState');

      if (d.state === 'IDLE') {
        if (timerInterval) {
          clearInterval(timerInterval);
          timerInterval = null;
        }
        raceStateEl.innerText = `起點就緒 ｜ 設定：${d.targetLaps || 1} 次折返 (${d.totalLegs || 2} 趟)，拍起點或按 GO`;
        raceStateEl.style.color = '#facc15';
        document.getElementById('timeMain').innerText = '00.00';
      } else if (d.state === 'RUNNING') {
        const lapNum = Math.ceil((d.currentLeg || 1) / 2);
        const isOutbound = ((d.currentLeg || 1) % 2 === 1);
        const isFinalLeg = (d.currentLeg === d.totalLegs);

        if (isOutbound) {
          raceStateEl.innerText = `⚡ 第 ${lapNum} 次【去程】 ➔ 衝向折返點 (Pod 1)！(趟 ${d.currentLeg}/${d.totalLegs})`;
          raceStateEl.style.color = '#38bdf8';
        } else {
          if (isFinalLeg) {
            raceStateEl.innerText = `🏁 最終衝刺【衝線】 ➔ 衝回終點起點 (Pod 0)！(趟 ${d.currentLeg}/${d.totalLegs})`;
            raceStateEl.style.color = '#ec4899';
          } else {
            raceStateEl.innerText = `🔥 第 ${lapNum} 次【回程】 ➔ 衝回起點 (Pod 0)！(趟 ${d.currentLeg}/${d.totalLegs})`;
            raceStateEl.style.color = '#f97316';
          }
        }

        if (!timerInterval) {
          startTime = Date.now() - (d.elapsed || 0);
          timerInterval = setInterval(() => {
            const sec = (Date.now() - startTime) / 1000;
            document.getElementById('timeMain').innerText = sec.toFixed(2);
          }, 30);
        }
      } else if (d.state === 'FINISH') {
        if (timerInterval) {
          clearInterval(timerInterval);
          timerInterval = null;
        }
        raceStateEl.innerText = `🏆 完賽衝線！總成績 ${(d.total / 1000).toFixed(2)} 秒 (${d.targetLaps} 次折返共 ${d.totalLegs} 趟)`;
        raceStateEl.style.color = '#34d399';
        if (d.total) document.getElementById('timeMain').innerText = (d.total / 1000).toFixed(2);
      }

      // 動態更新分段成績卡片
      renderLaps(d);
    }

    function renderLaps(d) {
      const container = document.getElementById('lapsContainer');
      const totalLegs = d.totalLegs || (d.targetLaps ? d.targetLaps * 2 : 2);
      const laps = d.laps || [];

      let html = '';
      for (let i = 0; i < totalLegs; i++) {
        const legNum = i + 1;
        const lapIdx = Math.ceil(legNum / 2);
        const isOutbound = (legNum % 2 === 1);
        const title = isOutbound ? `第 ${lapIdx} 次 去程 (Pod 1)` : `第 ${lapIdx} 次 回程 (Pod 0)`;
        const timeVal = (i < laps.length && laps[i] > 0) ? (laps[i] / 1000).toFixed(2) + 's' : '--.--';
        const isCurrent = (d.state === 'RUNNING' && d.currentLeg === legNum);
        const borderStyle = isCurrent ? 'border-color: #38bdf8; background: rgba(56, 189, 248, 0.1);' : '';

        html += `
          <div class="lap-box" style="${borderStyle}">
            <div class="lap-title">${title}</div>
            <div class="lap-val" style="${isCurrent ? 'color:#38bdf8;' : ''}">${timeVal}</div>
          </div>
        `;
      }
      container.innerHTML = html;
    }

    // 定期輪詢狀態 (雙保險，防止 SSE 中斷)
    function pollStatus() {
      fetch('/status').then(r => r.json()).then(d => {
        updateUI(d);
      }).catch(e => {});
    }
    setInterval(pollStatus, 400);
    pollStatus();

    // SSE 即時推播
    const evtSource = new EventSource('/events');
    evtSource.onmessage = function(e) {
      try {
        const d = JSON.parse(e.data);
        updateUI(d);
      } catch (err) {}
    };

    evtSource.onerror = function() {
      document.getElementById('connStatus').innerText = '○ 輪詢模式';
      document.getElementById('connStatus').style.color = '#f59e0b';
    };
    evtSource.onopen = function() {
      document.getElementById('connStatus').innerText = '● 即時連線';
      document.getElementById('connStatus').style.color = '#93c5fd';
    };
  </script>
</body>
</html>
)rawliteral";

#endif
