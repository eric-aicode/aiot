#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <Arduino.h>

// 前端完整 HTML/CSS/JS (具備頁簽/收闔面板、選手姓名/座號自動跳號、Excel CSV成績匯出)
const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-TW">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ArenaPod 電子折返跑競技計時大看板</title>
  <link href="https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@700&family=Orbitron:wght@700;900&family=Inter:wght@400;600;800&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg: #070b14; --card: rgba(16,26,48,0.85); --cyan: #00f2fe; --green: #00ff87; --gold: #ffb703; --red: #ff0055;
      --text: #f0f4fc; --muted: #7e8ea6;
    }
    * { margin:0; padding:0; box-sizing:border-box; user-select:none; }
    body {
      background: radial-gradient(circle at 50% 10%, #0d1a33 0%, #070b14 100%);
      color: var(--text); font-family: 'Inter', system-ui, -apple-system, sans-serif;
      min-height: 100vh; padding: 14px; display: flex; flex-direction: column; gap: 12px;
    }
    .header {
      display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 12px;
      background: var(--card); border: 1px solid rgba(0,242,254,0.22); border-radius: 12px; padding: 12px 20px;
    }
    .brand-wrap { display: flex; align-items: center; gap: 14px; flex-shrink: 0; min-width: max-content; }
    .author-logo {
      height: 48px; width: 48px; min-width: 48px; min-height: 48px; flex-shrink: 0;
      object-fit: contain; border-radius: 10px;
      background: rgba(255,255,255,0.06); padding: 2px;
      border: 1px solid rgba(0,242,254,0.35);
      filter: drop-shadow(0 0 10px rgba(0,242,254,0.4));
    }
    .author-sub { font-size: 0.75rem; color: var(--muted); letter-spacing: 0.5px; margin-top: 2px; white-space: nowrap; }
    .brand { font-family: 'Chakra Petch', sans-serif; font-size: 1.3rem; font-weight:700; line-height: 1.2; white-space: nowrap; }
    .brand span { color: var(--cyan); }
    .header-pills { display: flex; align-items: center; gap: 8px; flex-wrap: wrap; margin-left: auto; }
    
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

    .test-badge {
      display: inline-flex; align-items: center; gap: 5px; padding: 5px 12px;
      border-radius: 20px; font-size: 0.82rem; font-weight:700; background: rgba(255,183,3,0.15); color: var(--gold);
      border: 1px solid rgba(255,183,3,0.35); cursor: pointer; transition: 0.2s;
    }
    .test-badge:hover { background: var(--gold); color: #000; box-shadow: 0 0 10px rgba(255,183,3,0.4); }

    .group-badge {
      display: inline-flex; align-items: center; gap: 5px; padding: 5px 12px;
      border-radius: 20px; font-size: 0.82rem; font-weight:700; background: rgba(0,242,254,0.12); color: var(--cyan);
      border: 1px solid rgba(0,242,254,0.35); cursor: pointer; transition: 0.2s;
    }
    .group-badge:hover { background: var(--cyan); color: #000; box-shadow: 0 0 10px rgba(0,242,254,0.4); }

    .client-badge {
      display: inline-flex; align-items: center; gap: 4px; padding: 5px 10px;
      border-radius: 20px; font-size: 0.80rem; font-weight:600; background: rgba(255,255,255,0.06); color: var(--muted);
      border: 1px solid rgba(255,255,255,0.1);
    }

    /* 動態視角與比例調整 */
    .grid {
      display: grid; grid-template-columns: 1.7fr 1.1fr; gap: 14px; flex: 1;
      transition: grid-template-columns 0.3s cubic-bezier(0.4, 0, 0.2, 1);
    }
    .grid.mode-focus { grid-template-columns: 1fr 0px; }
    .grid.mode-focus .right-panel { display: none; }
    .grid.mode-focus .digits { font-size: clamp(4rem, 11vw, 8rem); }
    .grid.mode-balanced { grid-template-columns: 1.7fr 1.1fr; }
    .grid.mode-leaderboard { grid-template-columns: 1fr 1.8fr; }
    .grid.mode-leaderboard .digits { font-size: clamp(2.4rem, 5vw, 4.2rem); }
    .grid.mode-leaderboard .tbl-wrap { max-height: 600px; }

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
    @media(max-width:960px){
      .grid { grid-template-columns: 1fr !important; }
      .grid.mode-focus .right-panel { display: none; }
    }
    .card { background: var(--card); border: 1px solid rgba(255,255,255,0.08); border-radius: 16px; padding: 18px; display: flex; flex-direction: column; gap: 12px; }
    
    /* 選手輸入欄 */
    .athlete-bar {
      display: flex; align-items: center; justify-content: space-between; flex-wrap: wrap; gap: 8px;
      background: rgba(0,242,254,0.06); border: 1px solid rgba(0,242,254,0.22);
      border-radius: 12px; padding: 10px 14px;
    }
    .athlete-title { font-size: 0.9rem; font-weight: 700; color: var(--cyan); display: flex; align-items: center; gap: 6px; }
    .athlete-inputs { display: flex; align-items: center; gap: 8px; flex-wrap: wrap; }
    .athlete-input {
      background: #08101e; border: 1px solid rgba(255,255,255,0.18); border-radius: 8px;
      padding: 7px 11px; color: #fff; font-size: 0.9rem; outline: none; transition: 0.2s;
    }
    .athlete-input:focus { border-color: var(--cyan); box-shadow: 0 0 8px rgba(0,242,254,0.3); }
    .bib-input { width: 85px; font-weight: 800; text-align: center; color: var(--gold); }
    .name-input { width: 130px; font-weight: 600; }
    .bib-btn-group { display: flex; align-items: center; gap: 4px; }
    .btn-micro {
      background: rgba(255,255,255,0.08); border: 1px solid rgba(255,255,255,0.15);
      color: var(--text); padding: 6px 10px; border-radius: 6px; cursor: pointer; font-size: 0.8rem; font-weight: 700;
    }
    .btn-micro:hover { background: rgba(0,242,254,0.2); color: var(--cyan); }
    .auto-inc-label { font-size: 0.8rem; color: var(--muted); display: flex; align-items: center; gap: 4px; cursor: pointer; user-select: none; }

    /* 跑道與跑者 (零延遲極速響應，消除拖沓感) */
    .track { display: flex; align-items: center; background: rgba(0,0,0,0.3); border-radius: 10px; padding: 8px 16px; position: relative; }
    .track-line { flex:1; height: 8px; background: rgba(255,255,255,0.1); margin: 0 10px; border-radius: 4px; position: relative; }
    .track-prog { height: 100%; width: 0%; background: linear-gradient(90deg, var(--cyan), var(--green)); border-radius: 4px; }
    .runner { position: absolute; top: -20px; left: 0%; transform: translateX(-50%); font-size: 1.6rem; }
    .runner.left { transform: translateX(-50%) scaleX(-1); }
    .track-active {
      color: var(--cyan) !important;
      font-weight: 800;
      text-shadow: 0 0 12px var(--cyan), 0 0 20px var(--green);
      display: inline-block;
      animation: pulseTarget 0.5s ease-in-out infinite alternate;
    }
    @keyframes pulseTarget {
      from { transform: scale(1); opacity: 0.85; }
      to { transform: scale(1.1); opacity: 1; text-shadow: 0 0 22px var(--green); }
    }

    /* 巨型計時螢幕 (鎖定等寬數字，徹底消除計時跳動震動) */
    .timer-box {
      background: radial-gradient(circle, #0a172e 0%, #040812 100%);
      border: 2px solid rgba(0,242,254,0.3); border-radius: 16px; padding: 26px 10px;
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

    /* 右側頁簽導航 (Tabs) */
    .tabs-nav {
      display: flex; align-items: center; gap: 4px; border-bottom: 1px solid rgba(255,255,255,0.1);
      padding-bottom: 8px; position: relative;
    }
    .tab-btn {
      background: transparent; border: none; color: var(--muted); font-size: 0.88rem; font-weight: 700;
      padding: 7px 12px; border-radius: 8px; cursor: pointer; transition: 0.2s;
    }
    .tab-btn:hover { color: #fff; background: rgba(255,255,255,0.04); }
    .tab-btn.active {
      color: var(--cyan); background: rgba(0,242,254,0.12); border: 1px solid rgba(0,242,254,0.3);
    }
    .tab-btn-collapse {
      margin-left: auto; background: rgba(255,255,255,0.06); border: 1px solid rgba(255,255,255,0.12);
      color: var(--muted); padding: 5px 9px; border-radius: 6px; cursor: pointer; font-size: 0.8rem;
      transition: 0.2s;
    }
    .tab-btn-collapse:hover { color: #fff; background: rgba(255,255,255,0.15); }

    .tab-content { display: none; flex-direction: column; gap: 10px; }
    .tab-content.active { display: flex; }

    /* 排行榜工具列 */
    .toolbar { display: flex; gap: 6px; align-items: center; flex-wrap: wrap; }
    .btn-tool {
      padding: 6px 12px; border-radius: 8px; font-size: 0.82rem; font-weight: 700; border: none; cursor: pointer;
      display: inline-flex; align-items: center; gap: 4px; transition: 0.2s;
    }
    .btn-export { background: linear-gradient(135deg, #00ff87, #00b894); color: #020712; }
    .btn-export:hover { box-shadow: 0 0 10px rgba(0,255,135,0.4); }
    .btn-copy { background: rgba(0,242,254,0.15); color: var(--cyan); border: 1px solid rgba(0,242,254,0.3); }
    .btn-copy:hover { background: rgba(0,242,254,0.25); }
    .btn-clear { background: rgba(255,255,255,0.06); color: var(--muted); border: 1px solid rgba(255,255,255,0.1); margin-left: auto; }
    .btn-clear:hover { background: rgba(255,0,85,0.2); color: var(--red); border-color: rgba(255,0,85,0.4); }

    /* 排行榜表格 */
    .tbl-wrap { overflow-y: auto; max-height: 480px; }
    table { width: 100%; border-collapse: collapse; font-size: 0.88rem; text-align: left; }
    th { padding: 9px; background: rgba(0,0,0,0.3); color: var(--muted); position: sticky; top:0; font-size: 0.8rem; }
    td { padding: 9px; border-top: 1px solid rgba(255,255,255,0.05); }
    .rank-1 { color: #ffd700; font-weight:bold; }
    .rank-2 { color: #dcdde1; font-weight:bold; }
    .rank-3 { color: #e1b12c; font-weight:bold; }
    .hidden { display: none !important; }

    /* 設定頁面樣式 */
    .setting-item {
      display: flex; justify-content: space-between; align-items: center; background: rgba(255,255,255,0.03);
      padding: 10px 14px; border-radius: 10px; border: 1px solid rgba(255,255,255,0.06); font-size: 0.88rem;
    }
    .setting-item select {
      background: #0f1c33; color: var(--cyan); border: 1px solid rgba(0,242,254,0.4); padding: 5px 10px; border-radius: 6px; font-weight: bold; outline:none;
    }

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
      <img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGAAAABgCAYAAADimHc4AABWqUlEQVR42t39d3xd1ZX3j7/3KbcX9S7Zli33bjrGxvTeAiYhEBJSSCE9kwmTBGNSyUxCKiSh92I6oYOxwWAb9yoX2ZLV65Vuv6fu7x9HFjbhmUkymWee3+/8IcnyS/ecs9beq3zWZ60t+F+8pJTKqlUoS5bggnCP/D1QA0zrHDKir210LtJ0IYfTbmjLAXuhlAASEIf/YvRnyZwJypqisMiZtitOmyOfn1wVSKMNNsNd3UIsd4+4u7J48c3K6tW4cOTv/+9e4n9B6AJQhLhZHn5xXYDpysac5Zz95BqraVuLuWAwJWOdA26VrklFU4TmuJJQUHDcZJ8rkUd8HriuA4Df52NHu6o4+FAFmLZtWxbuxFqttyxKqrGGTWfNc/ZH9cFXVVF10D1CGbBUwAp3VJv//6cAKaVYtWqVumTJEhvAp4Jhy4ZtbeYZr26yzt3aYi/IGW75YFLajdVCzp/o0+ZN1ITPh5g3UQ0ihFQFQldV5YhPPeoVpCywdv0ud9vuFjk0OCSeeLUtv2NvmyQ0RSpF0+3xkxeI42bXaeUloQGlcGBT66anXs7tWPbGm5sK7RIQQkUuWqixerXzf0sR/+MKEAIef1yqS5cKZ1QRQeDUXz+T+cTGFvfknoRdUlGkipOn63LmeFWZPUENlERU35GPljOdMXGbliNBEA/rKMCBQ7288vo7vLNmA0MD/USCipg+pYG9bX20d3Ry6YUL8WGzeVcb2/cPm3u7fQXLN96tbDpVlFdNkHZ2IDHcvvbdvo23PYrZtQrIIxS4/DGVFUud/59WgJRSEcKz7VLK4J4u88qn1phf3NFqT+sZcq3zjvMpl53iD0yoUgIaqgAo2C6W7SIlUgjv+RRFIKXEcVziIR8Ab769gQcffpa21oPMnFLLOacvYO6sSdQ1VvP0U6s5uO8A1111BrFIAIGQCCGG0zlaWrtY+e52nntzu2zuoBCsO6tQOuFMN5fs1lM925vt/jf+mD74wuNjipA/VP4nfYT4nzI34mYEy4UrpfR1DDhX/+GFzOdXbrMnl0YV+/rzg/rZx+ixoK6qDpKc4eK6UioCIYRAfOipHNdF1zSCmuDtdVv49a/+TDqb4uOXLubSc06gpDREbrAPO5/k6Te2sWVHG9/77GJS6TyWZSMUgUDB59MIRyNEonGZsxSxZst+7l2xive2HnLi405PFU++2hrJ6Vqhf/0+q/PpOwebn3oIMFkmFZYL+T9hlv7pCnjrrbe0JUuW2AJwpTzh7teyv/jzi/nZJTGtcMNFAf85C3xxVVFE1nRwXIkQAuU/eQrHcYkGdVKZLN/9t39nx44dfPMLF3HFxQvBKtDTdoCRoUEUXJJ5iz8/+T43fvY0HMdGSlCE8KQmJVJKXNfFcRw0VSEej6H4g7y9qZVf3/8au9uG5PiZ5yed8suMtOkL0P/i9tTuP3030bVnnRAKUp6iwWr7/0kFSCnF0hUoK5YKR0pZvrbZ+PkvVuQuclwhv3R+UDtjvlakq6rIGA5SSlRFfMiFfuBWDz+Y7brEAjo7m1u45vP/ylmnTOcnN16Lptgc2ttMdmRwdLcolMTD/PnpDTTWFnPaMY2MZPL8lb8+0jEBjuMgXZd4LIgldW5/fC1/fnoNDbXlUhSfMJIMnWMLEMnddz4/vOvu7wEDV1zxhPrEE0vdw3r9f0IBR9t662PX/y73g037zIZrzgi6N1wYLFEVRckaDq6UKIr4iOj9gwdxD69S1yUSDNC8t4XLP/5lbv3+1Vy69Gz69+yir7MVRTqouo4iBIoiyGRN7nluI1//5MnYloNQPkLwH35zedjEedalpiLO8jvf5rePrmXi+CJ0VXOHfGclIhMuU1Jdm9tz2278cWZg71MgkPIm5ei84n9JAVJKVQjhSCkja3bnv/GbZ4zvGqbM3vrZSHRagx7+8IrnQ+nTB6bGQQiFcEDjsOyy+QJnXHAd//rli7jkkhPpbd6NnenH7/MjERiGSa5gEwnqvLLuAOmcwZVnzWQklUdVlL/vPQBFQKZgcduD79LcOkBPVlAWNhkyKrLlC76f7u7sCceSL/xizSd+8+vKG8g88cQH0d3/igIOP4CUcvxdr+Zuv/uVwimXnezPfuuyYLmiKEo6b0tN/cClyr8O3XFdFyEEEb8GwJYd+3l37Uaa97Wyf18rLQcOcdKCJpKyWh4A";
      <div>
        <div class="brand">ArenaPod <span>折返跑競技大看板</span></div>
        <div class="author-sub">AIoT 體適能智慧運動科技 · 王朝葦老師團隊</div>
      </div>
    </div>
    <div class="header-pills">
      <div class="view-switch">
        <button class="view-btn" id="btnViewFocus" onclick="setViewMode('focus')" title="專注計時：全螢幕放大計時">⏱ 專注</button>
        <button class="view-btn active" id="btnViewBalanced" onclick="setViewMode('balanced')" title="標準平衡：雙欄並重">⚖️ 平衡</button>
        <button class="view-btn" id="btnViewLeaderboard" onclick="setViewMode('leaderboard')" title="排行榜優先：展開完整排名">🏆 排行</button>
      </div>
      <div class="group-badge" id="badgeGroup" onclick="changeGroupModal()" title="點擊切換教室桌號/房間碼">🏷️ 第 <span id="valGroupId">1</span> 組</div>
      <button class="test-badge" onclick="openFindModal()" title="尋找實體盒：呼叫全體盒子或指定單盒爆閃嗶聲">🔔 尋找實體盒 ▾</button>
      <div class="distance-badge" id="badgeDistance">📡 距離測算中...</div>
      <button class="wifi-badge" id="badgeWifi" onclick="openWifiModal()">🌐 外部 Wi-Fi</button>
      <div class="client-badge" id="badgeClients" title="已連入此看板的多端裝置數">👥 1 台</div>
      <div class="status-badge" id="badgeStatus">待機就緒</div>
    </div>
  </div>

  <div class="grid mode-balanced" id="mainGrid">
    <!-- 左側主要舞台：計時、跑道、選手輸入與控制 -->
    <div class="card">
      <!-- 選手座號與姓名即時輸入列 -->
      <div class="athlete-bar">
        <div class="athlete-title">🏃 當前測驗選手：</div>
        <div class="athlete-inputs">
          <input type="text" id="inputAthleteBib" class="athlete-input bib-input" placeholder="座號/號碼" value="1" title="選手座號或號碼">
          <input type="text" id="inputAthleteName" class="athlete-input name-input" placeholder="姓名 (選填)" title="選手姓名 (選填)">
          <div class="bib-btn-group">
            <button class="btn-micro" onclick="stepAthlete(-1)" title="上一號">◀</button>
            <button class="btn-micro" onclick="stepAthlete(1)" title="下一號">▶</button>
          </div>
          <label class="auto-inc-label" title="完賽自動儲存並遞增座號">
            <input type="checkbox" id="chkAutoIncrement" checked> 自動跳號
          </label>
        </div>
      </div>

      <!-- 跑道與跑者動態軌跡 -->
      <div class="track">
        <span id="labelStart">🏁 起點 (Pod #0)</span>
        <div class="track-line">
          <div class="runner" id="runner">🏃</div>
          <div class="track-prog" id="prog"></div>
        </div>
        <span id="labelTurn">🚩 折返點 (Pod #1)</span>
      </div>

      <!-- 巨型等寬計時巨幕 -->
      <div class="timer-box">
        <div class="phase-tag" id="phaseTag">READY TO RACE</div>
        <div class="digits" id="digits">00:00<span>.000</span></div>
        <div class="overlay-count hidden" id="overlayCount">3</div>
      </div>

      <!-- 分段耗時與總成績卡片 -->
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

      <!-- 操作按鈕群 -->
      <div class="controls">
        <button class="btn btn-start" onclick="startWebCountdown()">⚡ 倒數發令起跑</button>
        <button class="btn btn-hit" onclick="onHitClick()">🥊 模擬按鈕拍擊</button>
        <button class="btn btn-reset" onclick="onResetClick()">🔄 歸零重置</button>
      </div>
    </div>

    <!-- 右側分頁與收闔面板：排行榜、設備雷達、測驗設定 -->
    <div class="card right-panel" id="rightPanel">
      <div class="tabs-nav">
        <button class="tab-btn active" id="tabBtnLeaderboard" onclick="switchTab('leaderboard')">🏆 測驗成績 (<span id="countRecords">0</span>)</button>
        <button class="tab-btn" id="tabBtnMesh" onclick="switchTab('mesh')">📡 設備雷達</button>
        <button class="tab-btn" id="tabBtnSettings" onclick="switchTab('settings')">⚙️ 設定</button>
        <button class="tab-btn-collapse" id="btnCollapsePanel" onclick="toggleCollapsePanel()" title="收闔 / 展開側欄">◀ 收闔</button>
      </div>

      <!-- 頁簽 1: 🏆 測驗成績排行榜 -->
      <div class="tab-content active" id="tabContentLeaderboard">
        <div class="toolbar">
          <button class="btn-tool btn-export" onclick="exportCsv()" title="匯出 Excel 相容 UTF-8 CSV 檔">📥 匯出 CSV</button>
          <button class="btn-tool btn-copy" onclick="copyLeaderboard()" title="複製文字表格到剪貼簿，可貼 LINE 或 Google Sheets">📋 複製成績</button>
          <button class="btn-tool btn-clear" onclick="clearHistory()">🗑️ 清空</button>
        </div>
        <div class="tbl-wrap">
          <table>
            <thead>
              <tr>
                <th style="width:15%;">名次</th>
                <th>選手 (座號/姓名)</th>
                <th style="width:18%;">趟數</th>
                <th style="width:25%;">總成績</th>
                <th style="width:12%; text-align:center;">刪除</th>
              </tr>
            </thead>
            <tbody id="tblBody">
              <tr><td colspan="5" style="text-align:center; color:#555; padding:20px;">尚無紀錄</td></tr>
            </tbody>
          </table>
        </div>
      </div>

      <!-- 頁簽 2: 📡 Pod Mesh 拓撲與設備雷達 -->
      <div class="tab-content" id="tabContentMesh">
        <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:6px;">
          <div style="font-weight:700; font-size:0.85rem; color:var(--cyan); display:flex; align-items:center; gap:6px;">
            <span>在線盒子列表</span>
            <span id="badgeEnrolling" class="hidden" style="background:var(--gold); color:#000; font-size:0.7rem; font-weight:800; padding:2px 8px; border-radius:10px;">收編中 (15s)</span>
          </div>
          <div style="display:flex; gap:6px;">
            <button class="btn-micro" style="background:rgba(0,255,135,0.18); color:var(--green); border-color:rgba(0,255,135,0.35);" onclick="openEnroll()">➕ 拍擊收編</button>
            <button class="btn-micro" style="background:rgba(255,0,85,0.15); color:var(--red); border-color:rgba(255,0,85,0.35);" onclick="unpairSystem()">⚠️ 重置全體</button>
          </div>
        </div>
        <div id="meshPodGrid" style="display:grid; grid-template-columns:repeat(auto-fit, minmax(130px, 1fr)); gap:8px;">
          <!-- 節點動態生成 -->
        </div>
      </div>

      <!-- 頁簽 3: ⚙️ 測驗拓撲與系統參數設定 -->
      <div class="tab-content" id="tabContentSettings">
        <div class="setting-item">
          <span>🏃 運動拓撲模式</span>
          <select id="selectTrackMode" onchange="changeTrackMode()">
            <option value="0" selected>雙點直線折返 (0 ⮂ 1)</option>
            <option value="1">三角繞錐折返 (0➔1➔2➔0)</option>
            <option value="2">四角障礙折返 (0➔1➔2➔3➔0)</option>
            <option value="3">隨機敏捷反應燈</option>
          </select>
        </div>
        <div class="setting-item">
          <span>🎯 目標折返趟數</span>
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
        <div class="setting-item">
          <span>🏷️ 教室組號 (防撞)</span>
          <button class="btn-sm" style="background:rgba(0,242,254,0.15); color:var(--cyan); border:1px solid rgba(0,242,254,0.35);" onclick="changeGroupModal()">切換桌號</button>
        </div>
        <div class="setting-item">
          <span>🌐 外部 Wi-Fi 設定</span>
          <button class="btn-sm" style="background:rgba(0,168,255,0.18); color:#00a8ff; border:1px solid rgba(0,168,255,0.35);" onclick="openWifiModal()">設定校園 Wi-Fi</button>
        </div>
        <div class="setting-item">
          <span>⚠️ 解除綁定重置</span>
          <button class="btn-sm" style="background:rgba(255,0,85,0.15); color:var(--red); border:1px solid rgba(255,0,85,0.35);" onclick="unpairSystem()">解除全組配對</button>
        </div>
      </div>
    </div>
  </div>

  <!-- Wi-Fi 設定彈跳視窗 -->
  <div class="modal-backdrop hidden" id="modalWifi">
    <div class="modal-box">
      <div class="modal-header">
        <h3>🌐 外部 Wi-Fi 設定 (雙 IP 大螢幕模式)</h3>
        <button class="modal-close" onclick="closeWifiModal()">&times;</button>
      </div>
      <div style="font-size:0.83rem; color:var(--muted); line-height:1.4;">
        主機連上學校 Wi-Fi 後，大螢幕打校園 IP 即可投影！<br>
        <strong style="color:var(--cyan);">本地熱點 (192.168.4.1) 依然並存有效，自動同步跳頻。</strong>
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

  <!-- 實體盒雷達呼叫彈窗 -->
  <div class="modal-backdrop hidden" id="modalFind">
    <div class="modal-box">
      <div class="modal-header">
        <h3>🔔 尋找實體盒雷達呼叫</h3>
        <button class="modal-close" onclick="closeFindModal()">&times;</button>
      </div>
      <div style="font-size:0.83rem; color:var(--muted); line-height:1.4;">
        點擊下方按鈕，指定的盒子將立即發出 2400Hz 高頻警示蜂鳴並 LED 爆閃 2 秒。
      </div>
      <div style="display:flex; flex-direction:column; gap:8px; margin-top:8px;" id="findList">
        <button class="btn-sm" style="background:linear-gradient(135deg, #ffb703, #ff8800); color:#000; font-weight:800; padding:10px; cursor:pointer;" onclick="triggerTestBlink(255)">
          🌟 呼叫全體盒子 (全班盒子一齊爆閃嗶聲)
        </button>
        <button class="btn-sm" style="background:rgba(0,242,254,0.15); color:var(--cyan); border:1px solid rgba(0,242,254,0.35); text-align:left; padding:10px; cursor:pointer;" onclick="triggerTestBlink(0)">
          🏠 呼叫起點主機 (Pod #0 · 本機)
        </button>
        <div id="dynamicFindSlaves" style="display:flex; flex-direction:column; gap:8px;"></div>
      </div>
    </div>
  </div>

  <script>
    let state = 'IDLE';
    let startT = 0;
    let animId = null;
    let totalLaps = 4; // 預設共 4 趟 (2 回合標準)
    let currentFrontLeg = 1;
    let lastFrontLegT = 0;
    let records = JSON.parse(localStorage.getItem('arenapod_shuttle_records') || '[]');

    // 選手座號切換
    const stepAthlete = (delta) => {
      const bibInput = document.getElementById('inputAthleteBib');
      let val = parseInt(bibInput.value) || 1;
      val = Math.max(1, val + delta);
      bibInput.value = val;
    };

    // 頁簽切換
    const switchTab = (tabName) => {
      document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
      if (tabName === 'leaderboard') {
        document.getElementById('tabBtnLeaderboard').classList.add('active');
        document.getElementById('tabContentLeaderboard').classList.add('active');
      } else if (tabName === 'mesh') {
        document.getElementById('tabBtnMesh').classList.add('active');
        document.getElementById('tabContentMesh').classList.add('active');
      } else if (tabName === 'settings') {
        document.getElementById('tabBtnSettings').classList.add('active');
        document.getElementById('tabContentSettings').classList.add('active');
      }
    };

    // 收闔 / 展開側欄
    let isPanelCollapsed = false;
    const toggleCollapsePanel = () => {
      isPanelCollapsed = !isPanelCollapsed;
      const g = document.getElementById('mainGrid');
      const btn = document.getElementById('btnCollapsePanel');
      if (isPanelCollapsed) {
        setViewMode('focus');
        if (btn) btn.textContent = '▶ 展開';
      } else {
        setViewMode('balanced');
        if (btn) btn.textContent = '◀ 收闔';
      }
    };

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

    const timerLoop = () => {
      if (state === 'RUNNING') {
        let el = performance.now() - startT;
        let s = Math.floor(el/1000), f = Math.floor(el%1000), m = Math.floor(s/60); s = s % 60;
        document.getElementById('digits').innerHTML = `${String(m).padStart(2,'0')}:${String(s).padStart(2,'0')}<span>.${String(f).padStart(3,'0')}</span>`;
        animId = requestAnimationFrame(timerLoop);
      }
    };

    const sendCmd = (act, extra = '') => {
      fetch('/cmd?action=' + act + extra).catch(e => console.warn(e));
    };

    let currentGroupId = 1;
    const changeGroupModal = () => {
      let g = prompt('請輸入教室桌號/房間碼 (1~99)：', currentGroupId);
      if (g) {
        let n = parseInt(g);
        if (n > 0) {
          currentGroupId = n;
          document.getElementById('valGroupId').textContent = n;
          sendCmd('set_group', '&gid=' + n);
        }
      }
    };

    const triggerTestBlink = (podId = 255) => {
      sendCmd('test_blink', '&pod=' + podId);
      playBeep(2400, 0.15);
      closeFindModal();
    };

    const openFindModal = () => document.getElementById('modalFind').classList.remove('hidden');
    const closeFindModal = () => document.getElementById('modalFind').classList.add('hidden');

    const openEnroll = () => {
      sendCmd('open_enroll');
      const b = document.getElementById('badgeEnrolling');
      if (b) {
        b.classList.remove('hidden');
        setTimeout(() => b.classList.add('hidden'), 15000);
      }
      alert('➕ 已開啟 15 秒收編窗口！請在 15 秒內拍擊新盒子的大按鈕以收編入隊！');
    };

    const changeTrackMode = () => {
      let mode = parseInt(document.getElementById('selectTrackMode').value);
      sendCmd('set_track', '&mode=' + mode);
    };

    let currentPods = [{ id: 0, role: 'MASTER', name: '起點主控 (Pod #0)', rssi: 0, online: true }];
    const updateMeshUI = (pods) => {
      if (Array.isArray(pods)) currentPods = pods;
      const grid = document.getElementById('meshPodGrid');
      if (grid) {
        grid.innerHTML = currentPods.map(p => `
          <div style="background:rgba(255,255,255,0.04); border:1px solid ${p.id===0 ? 'rgba(0,242,254,0.35)' : 'rgba(0,255,135,0.3)'}; border-radius:8px; padding:8px 10px; display:flex; flex-direction:column; gap:6px;">
            <div style="display:flex; justify-content:space-between; align-items:center;">
              <span style="font-weight:700; font-size:0.82rem; color:${p.id===0 ? 'var(--cyan)' : 'var(--green)'};">${p.name}</span>
              <span style="font-size:0.72rem; color:${p.rssi ? (p.rssi > -65 ? 'var(--green)' : 'var(--gold)') : 'var(--muted)'};">${p.rssi ? p.rssi + ' dBm' : '本機'}</span>
            </div>
            <button class="btn-micro" style="background:rgba(255,183,3,0.18); color:var(--gold); border-color:rgba(255,183,3,0.35); font-size:0.75rem; padding:4px 6px;" onclick="triggerTestBlink(${p.id})">
              🔔 尋找此盒
            </button>
          </div>
        `).join('');
      }

      const findSlavesEl = document.getElementById('dynamicFindSlaves');
      if (findSlavesEl) {
        const slaves = currentPods.filter(p => p.id !== 0);
        if (slaves.length === 0) {
          findSlavesEl.innerHTML = '<div style="font-size:0.78rem; color:var(--muted); padding:6px 0;">尚無連線中的競技子機</div>';
        } else {
          findSlavesEl.innerHTML = slaves.map(p => `
            <button class="btn-sm" style="background:rgba(0,255,135,0.14); color:var(--green); border:1px solid rgba(0,255,135,0.35); text-align:left; padding:8px 10px; cursor:pointer;" onclick="triggerTestBlink(${p.id})">
              🚩 呼叫 ${p.name} ${p.rssi ? `(${p.rssi} dBm)` : ''}
            </button>
          `).join('');
        }
      }
    };

    const unpairSystem = () => {
      if (confirm("⚠️ 確定要解除全體盒子配對並重置回出廠慢閃？\n\n重置後所有盒子將回到 800ms 慢閃狀態，按下哪一顆按鈕，該顆就會自動成為起點主機！")) {
        sendCmd('unpair');
        alert("✅ 已送出全體重置指令！全體盒子將重置為慢閃待命。");
        setTimeout(() => location.reload(), 1500);
      }
    };

    const changeLaps = () => {
      totalLaps = parseInt(document.getElementById('selectLaps').value);
      sendCmd('set_laps', '&laps=' + totalLaps);
    };

    // 核心事件處理：起跑
    const handleStartRaceEvent = (d) => {
      state = 'RUNNING';
      startT = performance.now();
      currentFrontLeg = 1;
      lastFrontLegT = startT;
      document.getElementById('overlayCount').classList.add('hidden');
      let targetPod = (d && d.targetPod !== undefined) ? d.targetPod : 1;
      let bib = document.getElementById('inputAthleteBib').value.trim() || '1';
      let name = document.getElementById('inputAthleteName').value.trim();
      let athleteStr = name ? `${bib}號 ${name}` : `${bib}號`;
      document.getElementById('phaseTag').textContent = `🏃 [${athleteStr}] 第 1 趟 / 共 ${totalLaps} 趟`;
      document.getElementById('valLeg1').textContent = `第 1 趟`;
      document.getElementById('valLeg2').textContent = '--:--.--';
      document.getElementById('valTotal').textContent = '--:--.--';
      document.getElementById('colTotal').classList.remove('gold');
      document.getElementById('badgeStatus').textContent = `衝向 Pod #${targetPod}`;
      let pct = targetPod === 1 ? '92%' : targetPod === 2 ? '50%' : '75%';
      document.getElementById('runner').style.left = pct;
      document.getElementById('prog').style.width = pct;
      document.getElementById('runner').classList.remove('left');

      if (targetPod === 0) {
        document.getElementById('labelStart').classList.add('track-active');
        document.getElementById('labelTurn').classList.remove('track-active');
      } else {
        document.getElementById('labelTurn').classList.add('track-active');
        document.getElementById('labelStart').classList.remove('track-active');
      }

      cancelAnimationFrame(animId);
      animId = requestAnimationFrame(timerLoop);
    };

    // 核心事件處理：換趟
    const handleLapEvent = (d) => {
      let cur = d.currentLeg;
      currentFrontLeg = cur;
      document.getElementById('valLeg1').textContent = `第 ${cur} 趟`;
      if (d.legMs) {
        document.getElementById('valLeg2').textContent = (d.legMs/1000).toFixed(2) + 's';
      }
      playBeep(2200, 0.08);

      let targetPod = d.targetPod !== undefined ? d.targetPod : ((cur % 2 === 1) ? 1 : 0);
      let targetDesc = targetPod === 0 ? '衝回起點 Pod #0' : `衝向 Pod #${targetPod}`;
      let bib = document.getElementById('inputAthleteBib').value.trim() || '1';
      let name = document.getElementById('inputAthleteName').value.trim();
      let athleteStr = name ? `${bib}號 ${name}` : `${bib}號`;
      document.getElementById('phaseTag').textContent = `🏃 [${athleteStr}] 第 ${cur} 趟 / 共 ${d.targetLegs || totalLaps} 趟`;
      document.getElementById('badgeStatus').textContent = targetDesc;

      if (targetPod === 0) {
        document.getElementById('runner').style.left = '0%';
        document.getElementById('prog').style.width = '0%';
        document.getElementById('runner').classList.add('left');
        document.getElementById('labelStart').classList.add('track-active');
        document.getElementById('labelTurn').classList.remove('track-active');
      } else {
        let pct = targetPod === 1 ? '92%' : targetPod === 2 ? '50%' : '75%';
        document.getElementById('runner').style.left = pct;
        document.getElementById('prog').style.width = pct;
        document.getElementById('runner').classList.remove('left');
        document.getElementById('labelTurn').classList.add('track-active');
        document.getElementById('labelStart').classList.remove('track-active');
      }
    };

    // 核心事件處理：完賽
    const handleFinishEvent = (d) => {
      state = 'FINISHED';
      cancelAnimationFrame(animId);
      document.getElementById('labelStart').classList.remove('track-active');
      document.getElementById('labelTurn').classList.remove('track-active');
      let tot = d.totalMs || Math.round(performance.now() - startT);
      let s = Math.floor(tot/1000), f = Math.floor(tot%1000), m = Math.floor(s/60); s = s % 60;
      document.getElementById('digits').innerHTML = `${String(m).padStart(2,'0')}:${String(s).padStart(2,'0')}<span>.${String(f).padStart(3,'0')}</span>`;

      if (d.lastLegMs) {
        document.getElementById('valLeg2').textContent = (d.lastLegMs/1000).toFixed(2) + 's';
      }
      document.getElementById('valTotal').textContent = (tot/1000).toFixed(2) + 's';
      document.getElementById('colTotal').classList.add('gold');
      document.getElementById('phaseTag').textContent = 'FINISHED (完賽)';
      document.getElementById('badgeStatus').textContent = '完賽結算！';
      playBeep(1800, 0.4);

      // 取得選手座號與姓名
      const bibInput = document.getElementById('inputAthleteBib');
      const nameInput = document.getElementById('inputAthleteName');
      let bibVal = bibInput.value.trim() || String(records.length + 1);
      let nameVal = nameInput.value.trim();
      let displayName = nameVal ? `${bibVal}號 ${nameVal}` : `${bibVal}號`;

      records.push({
        id: Date.now(),
        bib: bibVal,
        name: nameVal,
        displayName: displayName,
        laps: totalLaps,
        tot: tot,
        timeStr: new Date().toLocaleTimeString('zh-TW', { hour12: false })
      });
      localStorage.setItem('arenapod_shuttle_records', JSON.stringify(records));
      renderTbl();

      // 若開啟自動跳號，自動累加座號並清空姓名供下一位測驗
      if (document.getElementById('chkAutoIncrement').checked) {
        let curNum = parseInt(bibVal);
        if (!isNaN(curNum)) {
          bibInput.value = curNum + 1;
          nameInput.value = '';
        }
      }
    };

    // 核心事件處理：重置
    const handleResetEvent = () => {
      state = 'IDLE';
      cancelAnimationFrame(animId);
      document.getElementById('labelStart').classList.remove('track-active');
      document.getElementById('labelTurn').classList.remove('track-active');
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
    };

    // 前端 3-2-1 倒數發令
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
          handleStartRaceEvent({ currentLeg: 1, targetLegs: totalLaps });
          sendCmd('start'); // 同步通知硬體起跑
        }
      }, 1000);
    };

    // 前端拍擊按鈕點擊
    const onHitClick = () => {
      if (state === 'IDLE' || state === 'FINISHED') {
        handleStartRaceEvent({ currentLeg: 1, targetLegs: totalLaps });
      } else if (state === 'RUNNING') {
        let nowT = performance.now();
        let legMs = Math.round(nowT - lastFrontLegT);
        lastFrontLegT = nowT;
        if (currentFrontLeg < totalLaps) {
          currentFrontLeg++;
          handleLapEvent({ currentLeg: currentFrontLeg, targetLegs: totalLaps, legMs: legMs });
        } else {
          handleFinishEvent({ totalMs: Math.round(nowT - startT), lastLegMs: legMs, targetLegs: totalLaps });
        }
      }
      sendCmd('hit');
    };

    // 前端重置按鈕點擊
    const onResetClick = () => {
      handleResetEvent();
      sendCmd('reset');
    };

    // 渲染排行榜表格
    const renderTbl = () => {
      document.getElementById('countRecords').textContent = records.length;
      if (!records.length) {
        document.getElementById('tblBody').innerHTML = '<tr><td colspan="5" style="text-align:center; color:#555; padding:20px;">尚無紀錄</td></tr>';
        return;
      }
      records.sort((a,b) => a.tot - b.tot);
      document.getElementById('tblBody').innerHTML = records.map((r,i) => `
        <tr>
          <td class="${i===0?'rank-1':i===1?'rank-2':i===2?'rank-3':''}">${i===0?'🥇 1':i===1?'🥈 2':i===2?'🥉 3':i+1}</td>
          <td onclick="editRecordName(${r.id})" style="cursor:pointer;" title="點擊修改選手姓名">
            <strong style="color:var(--text);">${r.displayName || r.name}</strong> <span style="font-size:0.75rem; color:var(--muted);">✏️</span>
          </td>
          <td>${r.laps} 趟</td>
          <td style="color:var(--cyan); font-weight:bold; font-family:'Consolas',monospace;">${(r.tot/1000).toFixed(2)}s</td>
          <td style="text-align:center;">
            <button onclick="deleteRecord(${r.id})" style="background:none; border:none; color:var(--muted); cursor:pointer; font-size:0.85rem;" title="刪除此筆">❌</button>
          </td>
        </tr>
      `).join('');
    };

    // 編輯選手名稱
    const editRecordName = (id) => {
      let rec = records.find(r => r.id === id);
      if (!rec) return;
      let newName = prompt('請修改選手座號或姓名：', rec.displayName || rec.name);
      if (newName !== null && newName.trim() !== '') {
        rec.displayName = newName.trim();
        rec.name = newName.trim();
        localStorage.setItem('arenapod_shuttle_records', JSON.stringify(records));
        renderTbl();
      }
    };

    // 刪除單筆紀錄
    const deleteRecord = (id) => {
      if (confirm('確定刪除這筆選手紀錄？')) {
        records = records.filter(r => r.id !== id);
        localStorage.setItem('arenapod_shuttle_records', JSON.stringify(records));
        renderTbl();
      }
    };

    // 匯出 CSV 檔 (含 UTF-8 BOM，Excel 打開絕不亂碼)
    const exportCsv = () => {
      if (!records.length) {
        alert('尚無任何測驗成績可供匯出！');
        return;
      }
      let csv = '\uFEFF'; // Excel UTF-8 BOM
      csv += '名次,座號,姓名,完整名稱,完成趟數,總耗時(秒),總毫秒,測驗時間\r\n';
      let sorted = [...records].sort((a,b) => a.tot - b.tot);
      sorted.forEach((r, idx) => {
        let rank = idx + 1;
        let bib = `"${(r.bib || '').replace(/"/g, '""')}"`;
        let name = `"${(r.name || '').replace(/"/g, '""')}"`;
        let disp = `"${(r.displayName || r.name || '').replace(/"/g, '""')}"`;
        let laps = r.laps;
        let sec = (r.tot / 1000).toFixed(2);
        let ms = r.tot;
        let t = `"${r.timeStr || ''}"`;
        csv += `${rank},${bib},${name},${disp},${laps},${sec},${ms},${t}\r\n`;
      });

      const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      const now = new Date();
      const timeStamp = `${now.getFullYear()}${String(now.getMonth()+1).padStart(2,'0')}${String(now.getDate()).padStart(2,'0')}_${String(now.getHours()).padStart(2,'0')}${String(now.getMinutes()).padStart(2,'0')}`;
      a.href = url;
      a.download = `ArenaPod_體適能折返跑成績_${timeStamp}.csv`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    };

    // 複製榜單文字
    const copyLeaderboard = () => {
      if (!records.length) {
        alert('尚無紀錄可複製！');
        return;
      }
      let sorted = [...records].sort((a,b) => a.tot - b.tot);
      let text = `🏆 ArenaPod 折返跑成績單 (${new Date().toLocaleDateString()})\n--------------------------------\n`;
      sorted.forEach((r, idx) => {
        let medal = idx === 0 ? '🥇' : idx === 1 ? '🥈' : idx === 2 ? '🥉' : `${idx+1}.`;
        text += `${medal} ${r.displayName || r.name} - ${(r.tot/1000).toFixed(2)}s (${r.laps}趟)\n`;
      });
      text += `--------------------------------\n共 ${records.length} 位選手完成測驗`;

      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(() => alert('📋 成績已複製到剪貼簿！可直接貼上至 LINE 或 Google Sheets。'));
      } else {
        let ta = document.createElement('textarea');
        ta.value = text;
        document.body.appendChild(ta);
        ta.select();
        document.execCommand('copy');
        document.body.removeChild(ta);
        alert('📋 成績已複製到剪貼簿！');
      }
    };

    // 動態視角與比例調整
    const setViewMode = (mode) => {
      const g = document.getElementById('mainGrid');
      g.classList.remove('mode-focus', 'mode-balanced', 'mode-leaderboard');
      g.classList.add('mode-' + mode);

      document.querySelectorAll('.view-btn').forEach(b => b.classList.remove('active'));
      if (mode === 'focus') {
        document.getElementById('btnViewFocus').classList.add('active');
        isPanelCollapsed = true;
        const btn = document.getElementById('btnCollapsePanel');
        if (btn) btn.textContent = '▶ 展開';
      } else if (mode === 'leaderboard') {
        document.getElementById('btnViewLeaderboard').classList.add('active');
        isPanelCollapsed = false;
        switchTab('leaderboard');
        const btn = document.getElementById('btnCollapsePanel');
        if (btn) btn.textContent = '◀ 收闔';
      } else {
        document.getElementById('btnViewBalanced').classList.add('active');
        isPanelCollapsed = false;
        const btn = document.getElementById('btnCollapsePanel');
        if (btn) btn.textContent = '◀ 收闔';
      }

      localStorage.setItem('arenapod_view_mode', mode);
    };

    const clearHistory = () => {
      if(confirm('確定清空所有排行榜紀錄？')){ records = []; localStorage.removeItem('arenapod_shuttle_records'); renderTbl(); }
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
      try {
        const evt = new EventSource('/events');

        evt.addEventListener('start', e => {
          let d = JSON.parse(e.data);
          handleStartRaceEvent(d);
        });

        evt.addEventListener('lap', e => {
          let d = JSON.parse(e.data);
          handleLapEvent(d);
        });

        evt.addEventListener('finish', e => {
          let d = JSON.parse(e.data);
          handleFinishEvent(d);
        });

        evt.addEventListener('mesh_update', e => {
          try {
            let pods = JSON.parse(e.data);
            updateMeshUI(pods);
          } catch(err){}
        });

        evt.addEventListener('reset', e => {
          handleResetEvent();
          try {
            let d = JSON.parse(e.data);
            if (d && d.groupId) {
              currentGroupId = d.groupId;
              document.getElementById('valGroupId').textContent = d.groupId;
            }
            if (d && d.mesh) updateMeshUI(d.mesh);
            if (d && d.trackMode !== undefined) {
              document.getElementById('selectTrackMode').value = d.trackMode;
            }
          } catch(err){}
        });

        evt.addEventListener('telemetry', e => {
          let d = JSON.parse(e.data);
          if (d.mesh) {
            updateMeshUI(d.mesh);
            let slave1 = d.mesh.find(p => p.id === 1);
            if (slave1 && slave1.rssi) {
              let estM = Math.pow(10, (-45 - slave1.rssi) / (10 * 2.4)).toFixed(1);
              document.getElementById('badgeDistance').textContent = `📡 估算折返距: ~${estM}m (${slave1.rssi} dBm)`;
            }
          }
          if (d.distance) {
            document.getElementById('badgeDistance').textContent = `📡 估算距離：${d.distance} (${d.rssi} dBm)`;
          }
          if (d.externalIp !== undefined) {
            updateWifiBadge({ connected: !!d.externalIp, ip: d.externalIp, ssid: d.wifiSsid, channel: d.channel });
          }
          if (d.groupId) {
            currentGroupId = d.groupId;
            document.getElementById('valGroupId').textContent = d.groupId;
          }
          if (d.trackMode !== undefined) {
            document.getElementById('selectTrackMode').value = d.trackMode;
          }
          if (d.clients !== undefined) {
            document.getElementById('badgeClients').textContent = `👥 ${d.clients} 台同步`;
          }
        });
      } catch (err) {
        console.warn('SSE 初始化錯誤 (離線預覽中):', err);
      }
    };

    window.onload = () => {
      const savedMode = localStorage.getItem('arenapod_view_mode') || 'balanced';
      setViewMode(savedMode);
      changeLaps();
      updateMeshUI();
      renderTbl();
      initSSE();
      fetchWifiStatus();
    };
  </script>
</body>
</html>
)rawliteral";

#endif
