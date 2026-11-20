# 打造互動教室：AIoT 機電整合與智慧裝置設計 🏃‍♂️
### ArenaPod 電子折返跑無線計時與多節點體能競技系統 (Mesh & Wireless Timing System)

> **教案設計與主講**：高雄市科技領域輔導團分團團隊 ｜ 高雄市立福山國中 **王朝葦 老師（王董）**  
> **研習對象**：全國國中/高中科技領域教師（生活科技 ✕ 資訊科技 ✕ 體育跨科整合）  
> **核心技術**：ESP8266 (Wemos D1 mini)、ESP-NOW 無線網格、Captive Portal 自動認證彈窗、Server-Sent Events (SSE) 即時大看板、AI 輔助程式設計 (Vibe Coding with Antigravity)

---

## 🌟 課程核心亮點

1. **生科工藝 ✕ 資訊通訊**：以雷切外殼與街機微動大按鈕為載體，整合 ESP8266 微控制器與 ESP-NOW 極速無線通訊。
2. **0ms 毫秒級運動計時**：採中斷/下降緣零延遲觸發，搭配 350ms 機械彈跳濾波，徹底解決傳統碼表人工反應誤差與爭議。
3. **100% 離線可用 (Zero-Cloud)**：現場無需校園 Wi-Fi 路由器，ESP-NOW 底層隔空互傳 ＋ 內建 SoftAP (192.168.4.1) 自動彈出網頁看板，免裝 App 即開即用。
4. **階梯式進化教案 (Stage 1A ➔ Stage 3B)**：從單板手速挑戰、Web OTA 空中無線更新、ESP-NOW 雙板乒乓、WPS 自動防串台配對，一路邁向多節點星狀網格 (Tap-to-Enroll 拍擊收編與四點循序闖關跑)。

---

## 🗂️ 專案目錄與各階段導航

| 階段代號 | 專案目錄 | 主程式 | 二進位檔 (.bin) | 教學重點與功能特色 |
| :---: | :--- | :--- | :--- | :--- |
| **Stage 1A** | [`Stage1A_SpeedTapper/`](./Stage1A_SpeedTapper/) | `Stage1A_SpeedTapper.ino` | `firmware_Stage1A.bin` | **單板手速挑戰賽 (有線閉環標準版)**<br>• 微動開關防彈跳<br>• 內建 Web 儀表板 |
| **Stage 1B** | [`Stage1B_SpeedTapper/`](./Stage1B_SpeedTapper/) | `Stage1B_SpeedTapper.ino` | `firmware_Stage1B.bin` | **Web OTA 空中無線升級版**<br>• 歡樂 Do-Mi-Sol-Do 開機音<br>• 鎖定四顆螺絲免拆線無線更新韌體 |
| **Stage 2A** | [`Stage2A_EspNowGroupId/`](./Stage2A_EspNowGroupId/) | `Stage2A_EspNowGroupId.ino` | `firmware_Stage2A.bin` | **雙板 ESP-NOW 廣播組別過濾版**<br>• 隔空拍擊傳球遊戲<br>• 16-bit Group ID 班級防串台機制 |
| **Stage 2B** | [`Stage2B_EspNowAutoPair/`](./Stage2B_EspNowAutoPair/) | `Stage2B_EspNowAutoPair.ino` | `firmware_Stage2B.bin` | **雙板 WPS 自動配對版**<br>• 3.5 秒窗口雙擊互鎖 MAC<br>• 永久記憶免疫隔壁組干擾 |
| **Stage 3A** | [`Stage3A_ShuttleRun/`](./Stage3A_ShuttleRun/) | `Stage3A_ShuttleRun.ino` | `firmware_Stage3A.bin` | **雙板通用直線多趟折返跑**<br>• 單一韌體：誰先按按鈕誰是主控<br>• 0ms 下降緣觸發 ＋ Captive Portal 彈窗<br>• 支援 1~4 趟體適能折返跑 |
| **Stage 3B (主)** | [`Stage3B_Master/`](./Stage3B_Master/) | `Stage3B_Master.ino` | `firmware_Stage3B_Master.bin` | **多節點星狀主控閘道 (Pod 0)**<br>• 拍擊動態收編 (Tap-to-Enroll)<br>• 雷達呼叫 (Blink-to-Find)<br>• 1➔2➔3➔4 跑跑卡丁車循序闖關跑 |
| **Stage 3B (從)** | [`Stage3B_Slave/`](./Stage3B_Slave/) | `Stage3B_Slave.ino` | `firmware_Stage3B_Slave.bin` | **極簡邊緣敏捷子機 (Pod 1~3)**<br>• 雙拍心跳脈衝待命 (1000ms)<br>• 當前目標 80ms 爆亮快閃引導衝刺 |

---

## 🛠️ 硬體電路與分側集中安全腳位

採用 **Wemos D1 mini (ESP8266)**，杜邦線集中於板子右側，杜絕跨板短路：

| 腳位標記 | GPIO 代號 | 連接硬體元件與極性 | 軟體配置與安全職責 |
| :---: | :---: | :--- | :--- |
| **D3** | GPIO0 | 微動開關訊號腳 (Switch Pin) | `INPUT_PULLUP`（內部上拉，拍下接通 0V） |
| **D4** | GPIO2 | 微動開關 ＆ LED 共有負極 | `OUTPUT` LOW（軟體虛擬接地 0V） |
| **D5** | GPIO14 | 大型街機按鈕內部 LED 陽極 (＋) | `OUTPUT` HIGH/LOW（狀態指示燈） |
| **D6** | GPIO12 | 蜂鳴器陽極 (＋) | `tone()` 頻率驅動（開機音、確認嗶聲、警告低音） |
| **D7** | GPIO13 | 蜂鳴器陰極 (－) | 軟體虛擬接地；**靜音時切換為 INPUT 高阻抗消噪** |

---

## 🌐 核心研習文件快速連結

- 📄 **[線上課程講義 (curriculum_guide.html)](./curriculum_guide.html)**：包含完整的研習教案背景、AI 提示詞詠唱公式、實作 SOP 與排錯指南。
- 📺 **[現場投影大簡報 (workshop_presentation.html)](./workshop_presentation.html)**：講師現場授課簡報，內建 A/B 板邏輯互動模擬器與全螢幕投影視圖。
- 📜 **[折返跑深度技術手冊 (折返跑.md)](./折返跑.md)**：包含 ArenaPod 狀態機時序圖、通訊協定封包格式與運動科學裁決邏輯。

---

## 📜 授權條款 (License)

本研習教案與韌體程式碼由 **高雄市科技領域輔導團 王朝葦 老師（王董）** 開發製作，採用 [MIT License](LICENSE) 開源授權，歡迎全國各級學校科技與體育領域教育工作者推廣、修改與融入校訂課程使用。
