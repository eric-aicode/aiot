#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <Arduino.h>

const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-TW">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Speed Tapper 瘋狂手速極限挑戰賽</title>
  <link href="https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@700&family=Orbitron:wght@700;900&family=Inter:wght@400;600;800&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg: #070b14; --card: rgba(16,26,48,0.85); --cyan: #00f2fe; --green: #00ff87; --gold: #ffb703; --red: #ff0055;
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
      background: var(--card); border: 1px solid rgba(0,242,254,0.25); border-radius: 12px; padding: 12px 18px;
    }
    .brand { font-family: 'Chakra Petch', sans-serif; font-size: 1.3rem; font-weight:700; }
    .brand span { color: var(--cyan); }
    .author-sub { font-size: 0.75rem; color: var(--muted); }
    .badge {
      display: inline-flex; align-items: center; gap: 6px; padding: 5px 12px;
      border-radius: 20px; font-size: 0.82rem; font-weight:700; background: rgba(0,255,135,0.15); color: var(--green);
    }
    .grid { display: grid; grid-template-columns: 1fr; gap: 14px; }
    @media (min-width: 768px) { .grid { grid-template-columns: 1.6fr 1fr; } }
    .card {
      background: var(--card); border: 1px solid rgba(255,255,255,0.08); border-radius: 14px; padding: 18px;
      display: flex; flex-direction: column; gap: 14px;
    }
    .card-title {
      font-size: 1rem; font-weight: 800; color: var(--cyan); display: flex; align-items: center; gap: 8px;
      border-bottom: 1px solid rgba(255,255,255,0.08); padding-bottom: 8px;
    }
    /* 遊戲大儀表 */
    .arena-box {
      background: rgba(0,0,0,0.4); border: 2px solid rgba(0,242,254,0.3); border-radius: 12px;
      padding: 20px; text-align: center; display: flex; flex-direction: column; gap: 10px; position: relative;
    }
    .timer-digits {
      font-family: 'Orbitron', monospace; font-size: clamp(2.5rem, 8vw, 4rem); font-weight: 900;
      color: var(--gold); text-shadow: 0 0 15px rgba(255,183,3,0.5);
    }
    .hit-counter {
      font-family: 'Orbitron', monospace; font-size: clamp(4rem, 14vw, 7rem); font-weight: 900;
      color: var(--cyan); text-shadow: 0 0 25px rgba(0,242,254,0.6); line-height: 1;
    }
    .cps-badge {
      font-size: 1.1rem; font-weight: 800; color: var(--green); letter-spacing: 1px;
    }
    .progress-bar-wrap {
      width: 100%; height: 10px; background: rgba(255,255,255,0.1); border-radius: 5px; overflow: hidden;
    }
    .progress-bar {
      height: 100%; width: 0%; background: linear-gradient(90deg, var(--cyan), var(--green)); transition: width 0.1s linear;
    }
    /* 控制按鈕群 */
    .btn-group { display: flex; gap: 10px; flex-wrap: wrap; }
    .btn {
      flex: 1; min-width: 120px; padding: 12px 16px; border: none; border-radius: 10px; font-size: 1rem;
      font-weight: 800; cursor: pointer; transition: 0.15s; display: flex; justify-content: center; align-items: center; gap: 6px;
    }
    .btn:active { transform: scale(0.96); }
    .btn-start { background: linear-gradient(135deg, var(--cyan), #0072ff); color: #000; box-shadow: 0 0 15px rgba(0,242,254,0.4); }
    .btn-reset { background: rgba(255,255,255,0.1); color: var(--text); border: 1px solid rgba(255,255,255,0.2); }
    .btn-action { background: rgba(0,255,135,0.15); color: var(--green); border: 1px solid rgba(0,255,135,0.3); }
    .btn-action:hover { background: var(--green); color: #000; }
    /* 設定選擇鈕 */
    .time-selector { display: flex; gap: 8px; justify-content: center; align-items: center; }
    .time-btn {
      padding: 6px 14px; border-radius: 20px; border: 1px solid rgba(255,255,255,0.2); background: transparent;
      color: var(--muted); font-weight: 700; cursor: pointer; font-size: 0.85rem;
    }
    .time-btn.active {
      background: var(--gold); color: #000; border-color: var(--gold); box-shadow: 0 0 10px rgba(255,183,3,0.4);
    }
    /* 結算彈窗 */
    .modal-backdrop {
      position: fixed; inset: 0; background: rgba(0,0,0,0.75); backdrop-filter: blur(5px);
      display: flex; justify-content: center; align-items: center; z-index: 99; padding: 16px;
    }
    .modal-box {
      background: #111a2e; border: 2px solid var(--gold); border-radius: 16px; padding: 24px; max-width: 380px;
      width: 100%; text-align: center; display: flex; flex-direction: column; gap: 14px;
      box-shadow: 0 0 30px rgba(255,183,3,0.3);
    }
    .hidden { display: none !important; }
  </style>
</head>
<body>
  <div class="header">
    <div>
      <div class="brand">Speed Tapper <span>手速挑戰賽 (Stage 1B OTA 版)</span></div>
      <div class="author-sub">AIoT 單板教學專案 · 支援空中無線韌體更新</div>
    </div>
    <div style="display:flex; align-items:center; gap:8px;">
      <a href="/update" target="_blank" style="text-decoration:none; padding:5px 12px; border-radius:20px; font-size:0.82rem; font-weight:700; background:rgba(0,242,254,0.15); color:var(--cyan); border:1px solid rgba(0,242,254,0.4);">📶 Web OTA 更新頁面</a>
      <div class="badge" id="connBadge">🟢 盒子連線中</div>
    </div>
  </div>

  <div class="grid">
    <!-- 左側：狂按手速挑戰主戰場 -->
    <div class="card">
      <div class="card-title">🥊 瘋狂手速挑戰擂台</div>
      
      <div style="display:flex; justify-content:space-between; align-items:center;">
        <span style="font-size:0.85rem; color:var(--muted);">挑戰時限：</span>
        <div class="time-selector">
          <button class="time-btn" onclick="setDuration(5)">5 秒</button>
          <button class="time-btn active" onclick="setDuration(10)">10 秒 (標準)</button>
          <button class="time-btn" onclick="setDuration(15)">15 秒</button>
        </div>
      </div>

      <div class="arena-box">
        <div style="font-size:0.85rem; color:var(--muted); letter-spacing:1px;" id="gameStatusText">點擊下方按鈕或直接拍打大按鈕開始！</div>
        <div class="timer-digits" id="valTimer">10.00s</div>
        <div class="hit-counter" id="valHits">0</div>
        <div class="cps-badge" id="valCPS">CPS: 0.0 (次/秒)</div>
        <div class="progress-bar-wrap">
          <div class="progress-bar" id="progressBar"></div>
        </div>
      </div>

      <div class="btn-group">
        <button class="btn btn-start" id="btnStart" onclick="startGame()">⚡ 開始挑戰 (3-2-1)</button>
        <button class="btn btn-reset" onclick="resetGame()">🔄 重新計次</button>
      </div>
    </div>

    <!-- 右側：硬體雙向檢測測試面板 -->
    <div class="card">
      <div class="card-title">🛠️ 單板硬體雙向檢測</div>
      <div style="font-size:0.85rem; color:var(--muted); line-height:1.5;">
        從手機發送指令控制實體盒子的燈光與蜂鳴器，驗證硬體接線是否 100% 正常。
      </div>

      <div style="display:flex; flex-direction:column; gap:8px;">
        <span style="font-size:0.85rem; font-weight:700;">💡 大按鈕 LED 燈控制：</span>
        <div class="btn-group">
          <button class="btn btn-action" onclick="sendCmd('led_on')">點亮 LED</button>
          <button class="btn btn-action" onclick="sendCmd('led_off')">熄滅 LED</button>
          <button class="btn btn-action" onclick="sendCmd('led_strobe')">爆閃 2 秒</button>
        </div>
      </div>

      <div style="display:flex; flex-direction:column; gap:8px; margin-top:8px;">
        <span style="font-size:0.85rem; font-weight:700;">🔊 無源蜂鳴器音效測試：</span>
        <div class="btn-group">
          <button class="btn btn-action" onclick="sendCmd('beep_coin')">🎵 吃金幣音</button>
          <button class="btn btn-action" onclick="sendCmd('beep_fanfare')">🎺 勝利短曲</button>
          <button class="btn btn-action" onclick="sendCmd('beep_alert')">⚠️ 警報音</button>
        </div>
      </div>

      <div style="margin-top:auto; padding:10px; background:rgba(255,255,255,0.04); border-radius:8px; font-size:0.8rem; color:var(--muted);">
        💡 <strong>極速提示</strong>：拍擊大按鈕時，微動開關以微秒級捕獲，音效非阻塞播放，一秒按 15 下絕不漏算！
      </div>
    </div>
  </div>

  <!-- 完賽結算彈窗 -->
  <div class="modal-backdrop hidden" id="resultModal">
    <div class="modal-box">
      <h2 style="color:var(--gold);">🏆 挑戰結算！</h2>
      <div style="font-size:0.9rem; color:var(--muted);" id="resultDuration">10 秒極限測試</div>
      <div style="font-size:3.5rem; font-family:'Orbitron'; font-weight:900; color:var(--cyan);" id="resultHits">68</div>
      <div style="font-size:1.1rem; font-weight:800; color:var(--green);" id="resultCPS">每秒 6.8 次 (CPS)</div>
      <div style="font-size:1rem; font-weight:700; color:var(--gold); padding:8px; background:rgba(255,183,3,0.15); border-radius:8px;" id="resultTitle">
        ⚡ 音速狂人！
      </div>
      <button class="btn btn-start" onclick="closeModal()">再來一局！</button>
    </div>
  </div>

  <script>
    let selectedDuration = 10;
    let gameState = 'IDLE'; // IDLE, COUNTDOWN, RUNNING, FINISHED
    let totalHits = 0;
    let startTime = 0;
    let animId = null;

    const setDuration = (sec) => {
      if (gameState === 'RUNNING') return;
      selectedDuration = sec;
      document.querySelectorAll('.time-btn').forEach(b => b.classList.remove('active'));
      event.target.classList.add('active');
      document.getElementById('valTimer').textContent = sec.toFixed(2) + 's';
      resetGame();
    };

    const sendCmd = (act, extra = '') => {
      fetch('/cmd?action=' + act + extra).catch(e => console.warn(e));
    };

    const startGame = () => {
      if (gameState === 'RUNNING') return;
      gameState = 'COUNTDOWN';
      let cd = 3;
      const st = document.getElementById('gameStatusText');
      st.textContent = `⚡ 準備起跑... ${cd}`;
      st.style.color = 'var(--gold)';
      sendCmd('countdown');

      const timer = setInterval(() => {
        cd--;
        if (cd > 0) {
          st.textContent = `⚡ 準備起跑... ${cd}`;
        } else {
          clearInterval(timer);
          st.textContent = '🥊 瘋狂拍擊！！！';
          st.style.color = 'var(--red)';
          beginRunning();
        }
      }, 1000);
    };

    const beginRunning = () => {
      gameState = 'RUNNING';
      totalHits = 0;
      document.getElementById('valHits').textContent = '0';
      startTime = performance.now();
      sendCmd('start_game', '&dur=' + selectedDuration);
      cancelAnimationFrame(animId);
      animId = requestAnimationFrame(gameLoop);
    };

    const gameLoop = () => {
      if (gameState !== 'RUNNING') return;
      let elapsed = (performance.now() - startTime) / 1000;
      let remain = Math.max(0, selectedDuration - elapsed);
      document.getElementById('valTimer').textContent = remain.toFixed(2) + 's';
      
      let pct = Math.min(100, (elapsed / selectedDuration) * 100);
      document.getElementById('progressBar').style.width = pct + '%';

      let currentCps = elapsed > 0 ? (totalHits / elapsed).toFixed(1) : 0;
      document.getElementById('valCPS').textContent = `CPS: ${currentCps} (次/秒)`;

      if (remain <= 0) {
        finishGame();
      } else {
        animId = requestAnimationFrame(gameLoop);
      }
    };

    const finishGame = () => {
      gameState = 'FINISHED';
      cancelAnimationFrame(animId);
      document.getElementById('valTimer').textContent = '0.00s';
      document.getElementById('progressBar').style.width = '100%';
      document.getElementById('gameStatusText').textContent = '🏁 時間到！挑戰完成！';
      document.getElementById('gameStatusText').style.color = 'var(--green)';

      let cps = (totalHits / selectedDuration).toFixed(1);
      document.getElementById('resultDuration').textContent = `${selectedDuration} 秒極限測試`;
      document.getElementById('resultHits').textContent = totalHits;
      document.getElementById('resultCPS').textContent = `每秒 ${cps} 次 (CPS)`;

      let title = '🥊 手速新星！';
      if (cps >= 10.0) title = '🚀 神之領域！極限光速手！';
      else if (cps >= 7.5) title = '⚡ 音速狂人！鍵盤電競大師！';
      else if (cps >= 5.0) title = '🔥 節奏菁英！反應敏捷！';
      document.getElementById('resultTitle').textContent = title;
      document.getElementById('resultModal').classList.remove('hidden');
    };

    const closeModal = () => {
      document.getElementById('resultModal').classList.add('hidden');
      resetGame();
    };

    const resetGame = () => {
      gameState = 'IDLE';
      cancelAnimationFrame(animId);
      totalHits = 0;
      document.getElementById('valHits').textContent = '0';
      document.getElementById('valCPS').textContent = 'CPS: 0.0 (次/秒)';
      document.getElementById('valTimer').textContent = selectedDuration.toFixed(2) + 's';
      document.getElementById('progressBar').style.width = '0%';
      document.getElementById('gameStatusText').textContent = '點擊下方按鈕或直接拍打大按鈕開始！';
      document.getElementById('gameStatusText').style.color = 'var(--muted)';
      sendCmd('reset_game');
    };

    // Server-Sent Events (SSE) 即時接收硬體拍擊事件
    const initSSE = () => {
      if (!window.EventSource) return;
      const evt = new EventSource('/events');
      evt.onopen = () => {
        const badge = document.getElementById('connBadge');
        if (badge) {
          badge.textContent = '🟢 盒子已連線';
          badge.style.background = 'rgba(0,255,135,0.15)';
          badge.style.color = 'var(--green)';
        }
      };
      evt.onerror = () => {
        const badge = document.getElementById('connBadge');
        if (badge) {
          badge.textContent = '🔴 連線中斷重試中...';
          badge.style.background = 'rgba(255,0,85,0.15)';
          badge.style.color = 'var(--red)';
        }
      };
      evt.addEventListener('hit', e => {
        let d = JSON.parse(e.data);
        totalHits = d.hits;
        document.getElementById('valHits').textContent = totalHits;
        // 如果當前處於 IDLE，拍第一下自動起跑！
        if (gameState === 'IDLE') {
          beginRunning();
        }
      });
      evt.addEventListener('state', e => {
        let d = JSON.parse(e.data);
        if (d.action === 'finish') finishGame();
      });
    };

    window.onload = () => {
      initSSE();
      resetGame();
    };
  </script>
</body>
</html>
)rawliteral";

#endif
