/**
 * 電子折返跑 競技計時大看板 核心邏輯 (app.js)
 * 支援：Web Serial 串流通訊、Web Audio 合成音效、60FPS 毫秒高幀率動畫、排行榜儲存與 CSV 匯出
 */

// ========== 系統全域狀態 ==========
const RaceState = {
  IDLE: 'IDLE',
  COUNTDOWN: 'COUNTDOWN',
  LEG1: 'LEG1',
  LEG2: 'LEG2',
  FINISHED: 'FINISHED'
};

let currentState = RaceState.IDLE;
let raceStartTime = 0;
let leg1Duration = 0;
let leg2Duration = 0;
let totalDuration = 0;
let timerAnimId = null;
let isAudioEnabled = true;

// Web Serial 串列埠變數
let serialPort = null;
let serialReader = null;
let serialWriter = null;
let isSerialConnected = false;

// 歷史成績列表 (支援 LocalStorage)
let leaderboardData = JSON.parse(localStorage.getItem('shuttle_leaderboard') || '[]');

// ========== DOM 元素快取 ==========
const dom = {
  // 狀態與頂部控制
  systemStatusPill: document.getElementById('systemStatusPill'),
  statusText: document.getElementById('statusText'),
  btnConnectSerial: document.getElementById('btnConnectSerial'),
  serialBtnText: document.getElementById('serialBtnText'),
  btnAudioToggle: document.getElementById('btnAudioToggle'),
  iconAudioOn: document.getElementById('iconAudioOn'),
  iconAudioOff: document.getElementById('iconAudioOff'),
  btnFullscreen: document.getElementById('btnFullscreen'),

  // 計時巨幕與跑道
  stageTag: document.getElementById('stageTag'),
  timerMins: document.getElementById('timerMins'),
  timerSecs: document.getElementById('timerSecs'),
  timerMs: document.getElementById('timerMs'),
  countdownOverlay: document.getElementById('countdownOverlay'),
  countdownText: document.getElementById('countdownText'),
  runnerAvatar: document.getElementById('runnerAvatar'),
  trackProgress: document.getElementById('trackProgress'),
  nodeStart: document.getElementById('nodeStart'),
  nodeTurn: document.getElementById('nodeTurn'),

  // 分段成績卡
  cardLeg1: document.getElementById('cardLeg1'),
  timeLeg1: document.getElementById('timeLeg1'),
  subLeg1: document.getElementById('subLeg1'),
  cardLeg2: document.getElementById('cardLeg2'),
  timeLeg2: document.getElementById('timeLeg2'),
  subLeg2: document.getElementById('subLeg2'),
  cardTotal: document.getElementById('cardTotal'),
  timeTotal: document.getElementById('timeTotal'),
  subTotal: document.getElementById('subTotal'),

  // 控制按鈕
  btnStartCountdown: document.getElementById('btnStartCountdown'),
  btnInstantStart: document.getElementById('btnInstantStart'),
  btnSimulateHit: document.getElementById('btnSimulateHit'),
  hitBtnText: document.getElementById('hitBtnText'),
  btnReset: document.getElementById('btnReset'),

  // 排行榜與日誌
  leaderboardBody: document.getElementById('leaderboardBody'),
  btnExportCsv: document.getElementById('btnExportCsv'),
  btnClearHistory: document.getElementById('btnClearHistory'),
  terminalLog: document.getElementById('terminalLog'),
  telemetryNodeBadge: document.getElementById('telemetryNodeBadge')
};

// ========== Web Audio 合成音效引擎 (零外部依賴) ==========
let audioCtx = null;

function initAudioContext() {
  if (!audioCtx) {
    const AudioContext = window.AudioContext || window.webkitAudioContext;
    audioCtx = new AudioContext();
  }
  if (audioCtx.state === 'suspended') {
    audioCtx.resume();
  }
}

// 播放短音頻 (倒數音)
function playTone(freq, type = 'sine', duration = 0.08, gainVal = 0.25) {
  if (!isAudioEnabled) return;
  try {
    initAudioContext();
    const osc = audioCtx.createOscillator();
    const gain = audioCtx.createGain();
    osc.type = type;
    osc.frequency.setValueAtTime(freq, audioCtx.currentTime);
    gain.gain.setValueAtTime(gainVal, audioCtx.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + duration);
    osc.connect(gain);
    gain.connect(audioCtx.destination);
    osc.start();
    osc.stop(audioCtx.currentTime + duration);
  } catch (e) {
    console.warn('音效播放失敗:', e);
  }
}

// 倒數音 (短嗶)
function soundCountdown() {
  playTone(1500, 'sine', 0.09, 0.3);
}

// 起跑長鳴 (GO!)
function soundGo() {
  if (!isAudioEnabled) return;
  try {
    initAudioContext();
    const now = audioCtx.currentTime;
    // 雙振盪器合成競技哨音
    [2200, 2600].forEach(f => {
      const osc = audioCtx.createOscillator();
      const gain = audioCtx.createGain();
      osc.type = 'triangle';
      osc.frequency.setValueAtTime(f, now);
      gain.gain.setValueAtTime(0.3, now);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.4);
      osc.connect(gain);
      gain.connect(audioCtx.destination);
      osc.start();
      osc.stop(now + 0.4);
    });
  } catch (e) {}
}

// 折返雙音反饋 (Chirp)
function soundTurnaround() {
  playTone(2000, 'sine', 0.06, 0.25);
  setTimeout(() => playTone(2600, 'sine', 0.08, 0.3), 70);
}

// 完賽勝利和弦 (Major Chord Fanfare)
function soundVictory() {
  if (!isAudioEnabled) return;
  const notes = [523.25, 659.25, 783.99, 1046.5]; // C5, E5, G5, C6
  notes.forEach((freq, idx) => {
    setTimeout(() => {
      playTone(freq, 'triangle', 0.35, 0.3);
    }, idx * 110);
  });
}

// ========== 毫秒高幀率動畫計時引擎 ==========
function formatMsToComponents(ms) {
  const totalSec = Math.floor(ms / 1000);
  const fraction = Math.floor(ms % 1000);
  const minutes = Math.floor(totalSec / 60);
  const seconds = totalSec % 60;
  return {
    m: String(minutes).padStart(2, '0'),
    s: String(seconds).padStart(2, '0'),
    ms: String(fraction).padStart(3, '0')
  };
}

function formatDuration(ms) {
  const c = formatMsToComponents(ms);
  return `${c.m}:${c.s}.${c.ms}`;
}

// 60FPS 逐幀更新計時器與跑道
function updateTimerLoop() {
  if (currentState === RaceState.LEG1 || currentState === RaceState.LEG2) {
    const elapsed = performance.now() - raceStartTime;
    const c = formatMsToComponents(elapsed);
    dom.timerMins.textContent = c.m;
    dom.timerSecs.textContent = c.s;
    dom.timerMs.textContent = c.ms;

    // 動畫模擬跑道進度
    if (currentState === RaceState.LEG1) {
      // 假設每趟約 5~6 秒，動態推進
      const progressPercent = Math.min((elapsed / 5000) * 100, 92);
      dom.runnerAvatar.style.left = `${progressPercent}%`;
      dom.trackProgress.style.width = `${progressPercent}%`;
      dom.runnerAvatar.classList.remove('facing-left');
    } else {
      // 回程向左跑
      const leg2Elapsed = elapsed - leg1Duration;
      const progressPercent = Math.max(92 - (leg2Elapsed / 5000) * 92, 0);
      dom.runnerAvatar.style.left = `${progressPercent}%`;
      dom.trackProgress.style.width = `${progressPercent}%`;
      dom.runnerAvatar.classList.add('facing-left');
    }

    timerAnimId = requestAnimationFrame(updateTimerLoop);
  }
}

// ========== 狀態機轉換核心 ==========
function setState(newState) {
  currentState = newState;
  dom.systemStatusPill.className = 'status-pill';

  switch (newState) {
    case RaceState.IDLE:
      dom.systemStatusPill.classList.add('status-idle');
      dom.statusText.textContent = '待機就緒 (READY)';
      dom.stageTag.textContent = 'READY TO RACE';
      dom.hitBtnText.textContent = '拍擊折返點 [H]';
      break;

    case RaceState.COUNTDOWN:
      dom.systemStatusPill.classList.add('status-countdown');
      dom.statusText.textContent = '倒數發令中...';
      dom.stageTag.textContent = 'COUNTDOWN';
      break;

    case RaceState.LEG1:
      dom.systemStatusPill.classList.add('status-running');
      dom.statusText.textContent = '第 1 趟：去程衝刺中';
      dom.stageTag.textContent = 'LEG 1 (OUTBOUND)';
      dom.hitBtnText.textContent = '拍擊折返點 [H]';
      dom.nodeStart.classList.add('active');
      dom.nodeTurn.classList.remove('hit');
      break;

    case RaceState.LEG2:
      dom.systemStatusPill.classList.add('status-running');
      dom.statusText.textContent = '第 2 趟：回程衝刺中';
      dom.stageTag.textContent = 'LEG 2 (RETURN)';
      dom.hitBtnText.textContent = '拍擊衝線完賽 [F]';
      dom.nodeTurn.classList.add('hit');
      break;

    case RaceState.FINISHED:
      dom.systemStatusPill.classList.add('status-finished');
      dom.statusText.textContent = '🎉 完賽結算！';
      dom.stageTag.textContent = 'FINISH & RESULT';
      dom.hitBtnText.textContent = '再測一次 [R]';
      dom.nodeStart.classList.add('hit');
      break;
  }
}

// 1. 啟動倒數發令
function actionStartCountdown() {
  if (currentState !== RaceState.IDLE && currentState !== RaceState.FINISHED) return;
  actionReset(false);
  setState(RaceState.COUNTDOWN);
  dom.countdownOverlay.classList.remove('hidden');

  let count = 3;
  dom.countdownText.textContent = count;
  soundCountdown();
  logMessage(`[發令] 倒數 3...`, 'text-gold');

  // 若序列埠連線，同步發送 's' 給 D1 mini
  sendSerialCommand('s');

  const countInterval = setInterval(() => {
    count--;
    if (count > 0) {
      dom.countdownText.textContent = count;
      soundCountdown();
      logMessage(`[發令] 倒數 ${count}...`, 'text-gold');
    } else {
      clearInterval(countInterval);
      dom.countdownOverlay.classList.add('hidden');
      actionStartRace();
    }
  }, 1000);
}

// 2. 正式起跑
function actionStartRace() {
  dom.countdownOverlay.classList.add('hidden');
  setState(RaceState.LEG1);
  raceStartTime = performance.now();
  soundGo();
  logMessage(`[起跑] 鳴槍起跑！跑者衝向折返點...`, 'text-cyan');
  cancelAnimationFrame(timerAnimId);
  timerAnimId = requestAnimationFrame(updateTimerLoop);
}

// 3. 折返拍擊
function actionTurnaroundHit(nodeId = '#REMOTE') {
  if (currentState !== RaceState.LEG1) return;
  leg1Duration = performance.now() - raceStartTime;
  setState(RaceState.LEG2);
  soundTurnaround();

  // 定格第 1 趟時間卡片
  dom.cardLeg1.classList.add('locked');
  dom.timeLeg1.textContent = formatDuration(leg1Duration);
  dom.subLeg1.textContent = `折返節點: ${nodeId}`;

  dom.runnerAvatar.style.left = '92%';
  dom.runnerAvatar.classList.add('facing-left');

  logMessage(`[折返] 🚩 節點 ${nodeId} 拍擊成功！去程: ${formatDuration(leg1Duration)}`, 'text-green');
}

// 4. 衝線完賽
function actionFinishHit(nodeId = '#REMOTE') {
  if (currentState !== RaceState.LEG2) return;
  cancelAnimationFrame(timerAnimId);

  const now = performance.now();
  totalDuration = now - raceStartTime;
  leg2Duration = totalDuration - leg1Duration;
  setState(RaceState.FINISHED);
  soundVictory();

  // 更新顯示數字
  const c = formatMsToComponents(totalDuration);
  dom.timerMins.textContent = c.m;
  dom.timerSecs.textContent = c.s;
  dom.timerMs.textContent = c.ms;

  // 定格第 2 趟與總成績卡片
  dom.cardLeg2.classList.add('locked');
  dom.timeLeg2.textContent = formatDuration(leg2Duration);
  dom.subLeg2.textContent = `衝線節點: ${nodeId}`;

  dom.cardTotal.classList.add('locked');
  dom.timeTotal.textContent = formatDuration(totalDuration);
  dom.subTotal.textContent = `完賽結算！`;

  dom.runnerAvatar.style.left = '0%';
  dom.trackProgress.style.width = '0%';

  logMessage(`[完賽] 🏆 總時間: ${formatDuration(totalDuration)} (去程: ${formatDuration(leg1Duration)}, 回程: ${formatDuration(leg2Duration)})`, 'text-gold');

  // 新增至歷史排行榜
  addLeaderboardRecord(totalDuration, leg1Duration, leg2Duration, nodeId);
}

// 5. 系統重置
function actionReset(notifySerial = true) {
  cancelAnimationFrame(timerAnimId);
  setState(RaceState.IDLE);

  dom.countdownOverlay.classList.add('hidden');
  dom.timerMins.textContent = '00';
  dom.timerSecs.textContent = '00';
  dom.timerMs.textContent = '000';

  dom.cardLeg1.classList.remove('locked');
  dom.timeLeg1.textContent = '--:--.---';
  dom.subLeg1.textContent = '等待折返拍擊';

  dom.cardLeg2.classList.remove('locked');
  dom.timeLeg2.textContent = '--:--.---';
  dom.subLeg2.textContent = '等待衝線拍擊';

  dom.cardTotal.classList.remove('locked');
  dom.timeTotal.textContent = '--:--.---';
  dom.subTotal.textContent = '等待賽事結算';

  dom.runnerAvatar.style.left = '0%';
  dom.trackProgress.style.width = '0%';
  dom.runnerAvatar.classList.remove('facing-left');
  dom.nodeStart.className = 'track-node node-start active';
  dom.nodeTurn.className = 'track-node node-turn';

  if (notifySerial) {
    sendSerialCommand('r');
    logMessage(`[重置] 系統已就緒，等待下一位選手`, 'text-muted');
  }
}

// 快捷操作處理
function actionSimulateHit() {
  if (currentState === RaceState.IDLE) {
    actionStartCountdown();
  } else if (currentState === RaceState.LEG1) {
    actionTurnaroundHit('#WEB_TEST');
    sendSerialCommand('h');
  } else if (currentState === RaceState.LEG2) {
    actionFinishHit('#WEB_TEST');
    sendSerialCommand('f');
  } else if (currentState === RaceState.FINISHED) {
    actionReset();
  }
}

// ========== 歷史排行榜管理 ==========
function addLeaderboardRecord(total, leg1, leg2, nodeId) {
  const runnerNum = `選手 #${String(leaderboardData.length + 1).padStart(2, '0')}`;
  const record = {
    id: Date.now(),
    runner: runnerNum,
    leg1: leg1,
    leg2: leg2,
    total: total,
    node: nodeId,
    date: new Date().toLocaleTimeString()
  };

  leaderboardData.push(record);
  // 按總成績從小到大排序 (跑越快名次越高)
  leaderboardData.sort((a, b) => a.total - b.total);
  localStorage.setItem('shuttle_leaderboard', JSON.stringify(leaderboardData));
  renderLeaderboard();
}

function renderLeaderboard() {
  if (leaderboardData.length === 0) {
    dom.leaderboardBody.innerHTML = `
      <tr class="empty-row">
        <td colspan="5">尚無完賽紀錄，點擊「發令起跑」開始測驗</td>
      </tr>
    `;
    return;
  }

  dom.leaderboardBody.innerHTML = leaderboardData.map((item, idx) => {
    let rankBadge = `<span class="rank-badge">${idx + 1}</span>`;
    if (idx === 0) rankBadge = `<span class="rank-badge rank-1">🥇</span>`;
    else if (idx === 1) rankBadge = `<span class="rank-badge rank-2">🥈</span>`;
    else if (idx === 2) rankBadge = `<span class="rank-badge rank-3">🥉</span>`;

    return `
      <tr>
        <td>${rankBadge}</td>
        <td><strong>${item.runner}</strong></td>
        <td>${formatDuration(item.leg1)}</td>
        <td>${formatDuration(item.leg2)}</td>
        <td class="table-total">${formatDuration(item.total)}</td>
      </tr>
    `;
  }).join('');
}

// 匯出 CSV 成績單
function exportLeaderboardCsv() {
  if (leaderboardData.length === 0) {
    alert('目前尚無成績紀錄可供匯出！');
    return;
  }

  let csv = '\uFEFF名次,選手編號,第1趟去程,第2趟回程,總成績,測試時間\n';
  leaderboardData.forEach((row, i) => {
    csv += `${i + 1},${row.runner},${formatDuration(row.leg1)},${formatDuration(row.leg2)},${formatDuration(row.total)},${row.date}\n`;
  });

  const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `折返跑競賽成績單_${new Date().toISOString().slice(0, 10)}.csv`;
  a.click();
  URL.revokeObjectURL(url);
}

// ========== 序列埠日誌輸出 ==========
function logMessage(text, colorClass = 'text-muted') {
  const line = document.createElement('div');
  line.className = `log-line ${colorClass}`;
  const time = new Date().toLocaleTimeString();
  line.textContent = `[${time}] ${text}`;
  dom.terminalLog.appendChild(line);
  dom.terminalLog.scrollTop = dom.terminalLog.scrollHeight;
}

// ========== Web Serial API (直接讀寫插在電腦上的 D1 mini) ==========
async function connectSerialPort() {
  if (!('serial' in navigator)) {
    alert('您的瀏覽器不支援 Web Serial API，請使用 Google Chrome 或 Edge 瀏覽器！');
    return;
  }

  if (isSerialConnected) {
    // 斷開連線
    try {
      if (serialReader) await serialReader.cancel();
      if (serialWriter) await serialWriter.close();
      if (serialPort) await serialPort.close();
    } catch (e) {}
    isSerialConnected = false;
    dom.serialBtnText.textContent = '連線 D1 mini (USB)';
    dom.btnConnectSerial.className = 'btn btn-primary';
    dom.telemetryNodeBadge.textContent = '離線中';
    logMessage('已斷開 D1 mini 序列埠連線', 'text-red');
    return;
  }

  try {
    serialPort = await navigator.serial.requestPort();
    await serialPort.open({ baudRate: 115200 });

    isSerialConnected = true;
    dom.serialBtnText.textContent = '已連線 D1 mini';
    dom.btnConnectSerial.className = 'btn btn-ghost';
    dom.telemetryNodeBadge.textContent = 'USB 即時通訊';
    logMessage('✅ 成功連線 D1 mini 序列埠 (115200 鮑率)！', 'text-green');

    // 啟動讀取串流
    readSerialStream();
  } catch (err) {
    console.error('Serial 連線失敗:', err);
    logMessage(`❌ 序列埠連線失敗: ${err.message}`, 'text-red');
  }
}

// 串流解析序列埠封包
async function readSerialStream() {
  const textDecoder = new TextDecoderStream();
  const readableStreamClosed = serialPort.readable.pipeTo(textDecoder.writable);
  serialReader = textDecoder.readable.getReader();

  let buffer = '';

  try {
    while (true) {
      const { value, done } = await serialReader.read();
      if (done) break;
      if (value) {
        buffer += value;
        const lines = buffer.split('\n');
        buffer = lines.pop(); // 保留尚未成行的殘餘

        for (const line of lines) {
          handleIncomingSerialLine(line.trim());
        }
      }
    }
  } catch (error) {
    console.warn('序列埠讀取中斷:', error);
  } finally {
    serialReader.releaseLock();
  }
}

// 發送指令至 D1 mini
async function sendSerialCommand(cmd) {
  if (!serialPort || !serialPort.writable) return;
  try {
    const encoder = new TextEncoder();
    const writer = serialPort.writable.getWriter();
    await writer.write(encoder.encode(cmd + '\n'));
    writer.releaseLock();
  } catch (e) {
    console.warn('發送指令失敗:', e);
  }
}

// 解析來自 D1 mini 的日誌與 JSON DATA
function handleIncomingSerialLine(line) {
  if (!line) return;

  // 1. 檢查是否為 DATA 結構化遙測
  if (line.startsWith('DATA:')) {
    try {
      const jsonStr = line.substring(5);
      const data = JSON.parse(jsonStr);
      processSerialData(data);
      return;
    } catch (e) {
      console.warn('DATA JSON 解析失敗:', line);
    }
  }

  // 2. 顯示硬體常規日誌在終端
  if (line.includes('⚡') || line.includes('折返') || line.includes('拍擊')) {
    logMessage(line, 'text-green');
  } else if (line.includes('GO') || line.includes('起跑')) {
    logMessage(line, 'text-cyan');
  } else if (line.includes('完賽') || line.includes('成績')) {
    logMessage(line, 'text-gold');
  } else if (line.includes('本機節點 ID')) {
    logMessage(line, 'text-cyan');
    const idMatch = line.match(/#[0-9A-Fa-f]{4}/);
    if (idMatch) dom.telemetryNodeBadge.textContent = `主控 ${idMatch[0]}`;
  }
}

// 根據 D1 mini 遙測事件驅動大看板
function processSerialData(data) {
  switch (data.event) {
    case 'COUNTDOWN_START':
      setState(RaceState.COUNTDOWN);
      dom.countdownOverlay.classList.remove('hidden');
      break;

    case 'COUNTDOWN':
      dom.countdownOverlay.classList.remove('hidden');
      dom.countdownText.textContent = data.sec;
      soundCountdown();
      logMessage(`[硬體同步] 倒數 ${data.sec}...`, 'text-gold');
      break;

    case 'START':
      actionStartRace();
      break;

    case 'TURNAROUND':
      actionTurnaroundHit(data.node || '#NODE');
      break;

    case 'FINISH':
      actionFinishHit(data.node || '#NODE');
      break;

    case 'RESET':
      actionReset(false);
      break;
  }
}

// ========== 事件監聽綁定 ==========
function setupEventListeners() {
  // 按鈕
  dom.btnConnectSerial.addEventListener('click', connectSerialPort);
  dom.btnStartCountdown.addEventListener('click', actionStartCountdown);
  dom.btnInstantStart.addEventListener('click', () => {
    actionReset(false);
    actionStartRace();
    sendSerialCommand('g');
  });
  dom.btnSimulateHit.addEventListener('click', actionSimulateHit);
  dom.btnReset.addEventListener('click', () => actionReset(true));
  dom.btnExportCsv.addEventListener('click', exportLeaderboardCsv);
  dom.btnClearHistory.addEventListener('click', () => {
    if (confirm('確定要清空排行榜歷史紀錄嗎？')) {
      leaderboardData = [];
      localStorage.removeItem('shuttle_leaderboard');
      renderLeaderboard();
      logMessage('排行榜紀錄已清空', 'text-red');
    }
  });

  // 音效切換
  dom.btnAudioToggle.addEventListener('click', () => {
    isAudioEnabled = !isAudioEnabled;
    dom.iconAudioOn.classList.toggle('hidden', !isAudioEnabled);
    dom.iconAudioOff.classList.toggle('hidden', isAudioEnabled);
  });

  // 全螢幕切換
  dom.btnFullscreen.addEventListener('click', () => {
    if (!document.fullscreenElement) {
      document.documentElement.requestFullscreen().catch(() => {});
    } else {
      document.exitFullscreen().catch(() => {});
    }
  });

  // 鍵盤快捷鍵
  window.addEventListener('keydown', (e) => {
    // 避免在輸入框時觸發
    if (e.target.tagName === 'INPUT') return;

    if (e.code === 'Space') {
      e.preventDefault();
      actionSimulateHit();
    } else if (e.code === 'KeyG') {
      actionReset(false);
      actionStartRace();
      sendSerialCommand('g');
    } else if (e.code === 'KeyH' || e.code === 'KeyF') {
      actionSimulateHit();
    } else if (e.code === 'KeyR') {
      actionReset(true);
    }
  });
}

// 頁面初始化
window.addEventListener('DOMContentLoaded', () => {
  renderLeaderboard();
  setupEventListeners();
  logMessage('網頁計時巨幕初始化完成，支援空白鍵、G、H、F、R 快捷鍵！', 'text-cyan');
});
