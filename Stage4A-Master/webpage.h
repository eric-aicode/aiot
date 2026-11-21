#ifndef WEBPAGE_H
#define WEBPAGE_H

const char PAGE_INDEX[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-TW">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>⚡ ArenaPod Stage 4A 互動搶答大看板</title>
  <style>
    :root {
      --bg: #0b1120;
      --card-bg: rgba(30, 41, 59, 0.85);
      --border: rgba(148, 163, 184, 0.2);
      --primary: #38bdf8;
      --success: #10b981;
      --warning: #f59e0b;
      --danger: #ef4444;
      --text: #f8fafc;
      --text-muted: #94a3b8;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; }
    body {
      background: radial-gradient(circle at 50% 10%, #1e293b, var(--bg) 80%);
      color: var(--text);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", sans-serif;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 1rem;
    }
    .container {
      width: 100%;
      max-width: 900px;
      display: flex;
      flex-direction: column;
      gap: 1.25rem;
    }
    header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 0.75rem 1.25rem;
      background: var(--card-bg);
      border: 1px solid var(--border);
      border-radius: 1rem;
      backdrop-filter: blur(8px);
    }
    .brand { display: flex; align-items: center; gap: 0.6rem; }
    .brand h1 { font-size: 1.15rem; font-weight: 800; color: #fff; letter-spacing: 0.05em; }
    .badge {
      background: rgba(56, 189, 248, 0.2);
      color: var(--primary);
      font-size: 0.75rem;
      font-weight: 700;
      padding: 0.2rem 0.6rem;
      border-radius: 9999px;
      border: 1px solid rgba(56, 189, 248, 0.3);
    }
    .status-pill {
      display: flex;
      align-items: center;
      gap: 0.4rem;
      font-size: 0.85rem;
      font-weight: 600;
      color: var(--text-muted);
    }
    .status-dot {
      width: 10px; height: 10px;
      border-radius: 50%;
      background: #64748b;
      transition: background 0.3s;
    }
    .status-dot.online { background: var(--success); box-shadow: 0 0 10px var(--success); }

    /* 搶答狀態看板 Hero Card */
    .hero-card {
      background: var(--card-bg);
      border: 2px solid var(--border);
      border-radius: 1.5rem;
      padding: 2.2rem 1.5rem;
      text-align: center;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      gap: 1rem;
      position: relative;
      overflow: hidden;
      box-shadow: 0 10px 30px rgba(0,0,0,0.4);
      transition: all 0.3s ease;
    }
    .hero-card.state-idle {
      border-color: rgba(148, 163, 184, 0.4);
    }
    .hero-card.state-armed {
      border-color: var(--warning);
      box-shadow: 0 0 40px rgba(245, 158, 11, 0.35);
      animation: pulse-border 1.5s infinite;
    }
    .hero-card.state-locked {
      border-color: var(--success);
      box-shadow: 0 0 50px rgba(16, 185, 129, 0.45);
    }
    @keyframes pulse-border {
      0%, 100% { border-color: rgba(245, 158, 11, 0.5); }
      50% { border-color: rgba(245, 158, 11, 1); }
    }
    .hero-status-tag {
      font-size: 0.9rem;
      font-weight: 700;
      letter-spacing: 0.1em;
      text-transform: uppercase;
      padding: 0.3rem 1rem;
      border-radius: 9999px;
      background: rgba(255,255,255,0.08);
      color: var(--text-muted);
    }
    .state-armed .hero-status-tag { background: rgba(245, 158, 11, 0.2); color: var(--warning); }
    .state-locked .hero-status-tag { background: rgba(16, 185, 129, 0.2); color: var(--success); }

    .winner-name {
      font-size: 3rem;
      font-weight: 900;
      letter-spacing: 0.02em;
      color: #fff;
      line-height: 1.1;
      text-shadow: 0 2px 10px rgba(0,0,0,0.5);
    }
    .winner-time {
      font-size: 1.4rem;
      font-weight: 700;
      color: var(--primary);
      font-feature-settings: "tnum";
    }

    /* 控制按鈕組 */
    .control-grid {
      display: grid;
      grid-template-columns: 2fr 1fr;
      gap: 1rem;
    }
    .btn {
      appearance: none;
      border: none;
      padding: 1.1rem 1.5rem;
      font-size: 1.25rem;
      font-weight: 800;
      border-radius: 1rem;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 0.6rem;
      transition: all 0.15s ease;
      color: #fff;
      box-shadow: 0 4px 14px rgba(0,0,0,0.25);
    }
    .btn:active { transform: scale(0.97); }
    .btn-arm {
      background: linear-gradient(135deg, #10b981, #059669);
    }
    .btn-arm:hover { background: linear-gradient(135deg, #34d399, #10b981); }
    .btn-reset {
      background: linear-gradient(135deg, #3b82f6, #1d4ed8);
    }
    .btn-reset:hover { background: linear-gradient(135deg, #60a5fa, #2563eb); }

    /* 搶答榜單與計分板 */
    .row-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 1.25rem;
    }
    @media (max-width: 700px) {
      .row-grid { grid-template-columns: 1fr; }
      .winner-name { font-size: 2.2rem; }
    }
    .card {
      background: var(--card-bg);
      border: 1px solid var(--border);
      border-radius: 1rem;
      padding: 1.25rem;
      display: flex;
      flex-direction: column;
      gap: 0.8rem;
    }
    .card-title {
      font-size: 1rem;
      font-weight: 700;
      color: #fff;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .rank-list {
      display: flex;
      flex-direction: column;
      gap: 0.5rem;
    }
    .rank-item {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 0.65rem 0.9rem;
      background: rgba(15, 23, 42, 0.6);
      border: 1px solid rgba(255,255,255,0.06);
      border-radius: 0.65rem;
      font-weight: 600;
    }
    .rank-item.first {
      background: rgba(16, 185, 129, 0.15);
      border-color: rgba(16, 185, 129, 0.4);
      color: #34d399;
    }
    .rank-badge {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      width: 24px; height: 24px;
      border-radius: 50%;
      background: rgba(255,255,255,0.1);
      font-size: 0.8rem;
      font-weight: 800;
      margin-right: 0.5rem;
    }
    .first .rank-badge { background: #10b981; color: #fff; }

    /* 計分板按鈕 */
    .score-table {
      display: flex;
      flex-direction: column;
      gap: 0.5rem;
    }
    .score-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 0.5rem 0.8rem;
      background: rgba(15, 23, 42, 0.6);
      border-radius: 0.65rem;
      border: 1px solid rgba(255,255,255,0.06);
    }
    .score-name { font-weight: 700; font-size: 0.95rem; }
    .score-actions { display: flex; align-items: center; gap: 0.5rem; }
    .score-val {
      min-width: 38px;
      text-align: center;
      font-size: 1.25rem;
      font-weight: 800;
      color: #facc15;
    }
    .btn-sm {
      width: 28px; height: 28px;
      border-radius: 6px;
      border: none;
      background: rgba(255,255,255,0.15);
      color: #fff;
      font-size: 1rem;
      font-weight: 800;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      justify-content: center;
    }
    .btn-sm:hover { background: rgba(255,255,255,0.3); }

    /* 快捷鍵提示 */
    .shortcuts-hint {
      text-align: center;
      font-size: 0.8rem;
      color: var(--text-muted);
      margin-top: 0.5rem;
    }
    kbd {
      background: #334155;
      padding: 0.15rem 0.4rem;
      border-radius: 4px;
      border: 1px solid #475569;
      color: #e2e8f0;
      font-size: 0.75rem;
    }
  </style>
</head>
<body>

<div class="container">
  <header>
    <div class="brand">
      <span class="badge">Stage 4A</span>
      <h1>ArenaPod 互動搶答大看板</h1>
    </div>
    <div class="status-pill">
      <div class="status-dot" id="connDot"></div>
      <span id="connText">連線中...</span>
    </div>
  </header>

  <!-- 搶答主看板 -->
  <div class="hero-card state-idle" id="heroCard">
    <div class="hero-status-tag" id="statusTag">⏳ 等待出題 (STANDBY)</div>
    <div class="winner-name" id="winnerName">請點擊【開放搶答】</div>
    <div class="winner-time" id="winnerTime">主機與全體子機皆可按鈕搶答！</div>
  </div>

  <!-- 主持人控制按鈕 -->
  <div class="control-grid">
    <button class="btn btn-arm" id="btnArm" onclick="armRound()">
      <span>🔔</span> 開放搶答 (GO!)
    </button>
    <button class="btn btn-reset" id="btnReset" onclick="resetRound()">
      <span>🔄</span> 重置下一題
    </button>
  </div>

  <div class="shortcuts-hint">
    鍵盤快捷鍵：<kbd>Space 空白鍵</kbd> 開放搶答 ｜ <kbd>R 鍵</kbd> 重置下一題
  </div>

  <div class="row-grid">
    <!-- 搶答名次排行榜 -->
    <div class="card">
      <div class="card-title">
        <span>🏁 搶答名次榜</span>
        <span style="font-size:0.75rem; color:var(--text-muted);">微秒級仲裁</span>
      </div>
      <div class="rank-list" id="rankList">
        <div style="text-align:center; color:var(--text-muted); padding:1rem; font-size:0.85rem;">
          本題尚未產生搶答紀錄
        </div>
      </div>
    </div>

    <!-- 即時計分板 -->
    <div class="card">
      <div class="card-title">
        <span>📊 各組計分板</span>
        <button class="btn-sm" onclick="resetScores()" title="分數歸零" style="width:auto; padding:0 0.4rem; font-size:0.7rem;">歸零</button>
      </div>
      <div class="score-table" id="scoreTable">
        <!-- 動態渲染各組分數 -->
      </div>
    </div>
  </div>
</div>

<script>
  let currentState = "IDLE";
  let scores = {
    "👑 主控台 (Pod 0)": 0,
    "Pod 1 (第 1 組)": 0,
    "Pod 2 (第 2 組)": 0,
    "Pod 3 (第 3 組)": 0
  };

  const audioCtx = new (window.AudioContext || window.webkitAudioContext)();
  function playTone(freq, dur, type='sine') {
    try {
      if (audioCtx.state === 'suspended') audioCtx.resume();
      const osc = audioCtx.createOscillator();
      const gain = audioCtx.createGain();
      osc.type = type;
      osc.frequency.setValueAtTime(freq, audioCtx.currentTime);
      gain.gain.setValueAtTime(0.25, audioCtx.currentTime);
      gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + dur);
      osc.connect(gain);
      gain.connect(audioCtx.destination);
      osc.start();
      osc.stop(audioCtx.currentTime + dur);
    } catch(e) {}
  }

  function playWinFanfare() {
    playTone(523.25, 0.1, 'triangle'); // C5
    setTimeout(() => playTone(659.25, 0.1, 'triangle'), 100); // E5
    setTimeout(() => playTone(783.99, 0.12, 'triangle'), 200); // G5
    setTimeout(() => playTone(1046.50, 0.35, 'square'), 320); // C6
  }

  function playArmBeep() {
    playTone(1760, 0.12, 'sine');
  }

  // 渲染計分板
  function renderScores() {
    const container = document.getElementById('scoreTable');
    container.innerHTML = '';
    for (let [name, val] of Object.entries(scores)) {
      const row = document.createElement('div');
      row.className = 'score-row';
      row.innerHTML = `
        <span class="score-name">${name}</span>
        <div class="score-actions">
          <button class="btn-sm" onclick="changeScore('${name}', -1)">−</button>
          <span class="score-val">${val}</span>
          <button class="btn-sm" onclick="changeScore('${name}', 1)">＋</button>
        </div>
      `;
      container.appendChild(row);
    }
  }

  function changeScore(name, delta) {
    scores[name] = Math.max(0, (scores[name] || 0) + delta);
    renderScores();
  }

  function resetScores() {
    if (confirm('確定將所有組別分數歸零嗎？')) {
      for (let k in scores) scores[k] = 0;
      renderScores();
    }
  }

  // 控制指令
  function armRound() {
    playArmBeep();
    fetch('/api/arm').catch(e => console.error(e));
  }

  function resetRound() {
    fetch('/api/reset').catch(e => console.error(e));
  }

  // 鍵盤快捷鍵
  window.addEventListener('keydown', (e) => {
    if (e.code === 'Space') {
      e.preventDefault();
      armRound();
    } else if (e.key === 'r' || e.key === 'R') {
      resetRound();
    }
  });

  // SSE 連線
  function initSSE() {
    const evtSource = new EventSource('/events');
    const heroCard = document.getElementById('heroCard');
    const statusTag = document.getElementById('statusTag');
    const winnerName = document.getElementById('winnerName');
    const winnerTime = document.getElementById('winnerTime');
    const connDot = document.getElementById('connDot');
    const connText = document.getElementById('connText');
    const rankList = document.getElementById('rankList');

    evtSource.onopen = () => {
      connDot.classList.add('online');
      connText.innerText = '已連線 (Online)';
    };

    evtSource.onerror = () => {
      connDot.classList.remove('online');
      connText.innerText = '連線中斷，重連中...';
    };

    evtSource.onmessage = (e) => {
      try {
        const data = JSON.parse(e.data);
        const state = data.state;

        if (state !== currentState) {
          if (state === 'ARMED') playArmBeep();
          if (state === 'LOCKED') playWinFanfare();
          currentState = state;
        }

        heroCard.className = 'hero-card state-' + state.toLowerCase();

        if (state === 'IDLE') {
          statusTag.innerText = '⏳ 等待出題 (STANDBY)';
          winnerName.innerText = '請點擊【開放搶答】';
          winnerTime.innerText = '主機與全體子機皆可按鈕搶答！';
          rankList.innerHTML = `<div style="text-align:center; color:var(--text-muted); padding:1rem; font-size:0.85rem;">等待開題中...</div>`;
        } else if (state === 'ARMED') {
          statusTag.innerText = '⚡ 搶答中 (ARMED)';
          winnerName.innerText = '🔥 請拍擊按鈕！';
          winnerTime.innerText = '誰先拍下，誰就是第一名！';
          rankList.innerHTML = `<div style="text-align:center; color:var(--warning); padding:1rem; font-weight:700;">搶答進行中，等待第一位拍擊者...</div>`;
        } else if (state === 'LOCKED') {
          statusTag.innerText = '🏆 搶答成功 (LOCKED)';
          winnerName.innerText = data.winnerName || '優勝者';
          winnerTime.innerText = `反應時間：${(data.reactionTime / 1000).toFixed(3)} 秒`;

          // 自動為第一名對應的計分板項目加上高亮提示
          if (scores[data.winnerName] !== undefined && data.isNewLock) {
            scores[data.winnerName] = (scores[data.winnerName] || 0) + 1;
            renderScores();
          }

          // 渲染名次列表
          if (data.rankings && data.rankings.length > 0) {
            rankList.innerHTML = '';
            data.rankings.forEach((r, idx) => {
              const item = document.createElement('div');
              item.className = 'rank-item' + (idx === 0 ? ' first' : '');
              const diffText = idx === 0 ? `${(r.time / 1000).toFixed(3)}s` : `+${((r.time - data.rankings[0].time) / 1000).toFixed(3)}s`;
              item.innerHTML = `
                <div>
                  <span class="rank-badge">${idx + 1}</span>
                  <span>${r.name}</span>
                </div>
                <span style="font-feature-settings: 'tnum'; font-weight:700;">${diffText}</span>
              `;
              rankList.appendChild(item);
            });
          }
        }
      } catch(err) {
        console.error(err);
      }
    };
  }

  window.addEventListener('DOMContentLoaded', () => {
    renderScores();
    initSSE();
  });
</script>

</body>
</html>
)rawliteral";

#endif
