#include <SPI.h>
#include <RF24.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>  // 引入持久化存储库

/*
1	GND	    GND         共地，必须硬连接
2	VCC	    3.3V        仅3.3V供电，禁止接5V，建议独立供电
3	(CE)	    22          NRF24模式控制引脚
4	(CSN)	21          NRF24 SPI片选引脚
5	(SCK)	18          ESP32 HSPI时钟引脚
6	(MOSI)	23          ESP32 HSPI发送引脚
7	(MISO)	19          ESP32 HSPI接收引脚
8	IRQ *	4           NRF24中断引脚（本代码悬空，暂未使用）
*/
// ================= 配置区域 =================
// Wi-Fi 设置
const char* ssid = "TP-LINK";
const char* password = "sb415sb415";

// NRF24 设置
#define CE_PIN  22
#define CSN_PIN 21
RF24 radio(CE_PIN, CSN_PIN);

const byte rxAddress[5] = {0x11, 0x22, 0x33, 0x44, 0x55};
const uint8_t PAYLOAD_SIZE = 32;
char receiveBuffer[PAYLOAD_SIZE + 1] = {0};

// WebServer 对象，端口 80
WebServer server(80);

// 仓位最大数量
#define MAX_BINS  8

// 单个仓位结构
struct BinInfo {
  uint8_t  id;
  float    weight;
  uint32_t item;
  uint32_t lastItem;
};

BinInfo bins[MAX_BINS];
uint8_t binCount = 0;         // 当前有效仓位数量

// 单条日志
struct LogEntry {
  uint8_t  binId;
  int32_t  delta;
  uint32_t itemNew;
  uint32_t time;              // 秒数
};

#define MAX_LOGS  64
LogEntry logs[MAX_LOGS];
uint8_t logCount = 0;
uint32_t logHead = 0;         // 循环队列头索引
uint32_t logTail = 0;         // 循环队列尾索引

// 持久化存储对象（命名空间：bin_manager，避免与其他程序冲突）
Preferences prefs;
#define PREFS_NAMESPACE "bin_manager"
#define PREFS_BIN_COUNT "bin_cnt"
#define PREFS_BIN_PREFIX "bin_"
// ===========================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("\n🚀 ESP32 仓位管理系统（持久化+网页编辑版）..."));

  // 1. 初始化持久化存储并加载仓位数据
  initPersistence();
  loadBinsFromFlash();

  // 2. 初始化 Wi-Fi
  initWiFi();

  // 3. 初始化 NRF24
  if (!radio.begin()) {
    Serial.println(F("❌ NRF24L01 硬件未响应！"));
    while (1);
  }
  Serial.println(F("✅ NRF24L01 初始化成功"));

  radio.openReadingPipe(1, rxAddress);
  radio.setChannel(40);
  radio.setPayloadSize(PAYLOAD_SIZE);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setCRCLength(RF24_CRC_8);
  radio.startListening();

  // 4. 初始化 Web Server 路由（新增编辑仓位路由）
  setupWebServer();

  Serial.println(F("🌐 系统就绪！请在浏览器输入 IP 地址访问。"));
}

void loop() {
  server.handleClient();

  uint8_t pipeNum;
  if (radio.available(&pipeNum)) {
    memset(receiveBuffer, 0, sizeof(receiveBuffer));
    radio.read(&receiveBuffer, PAYLOAD_SIZE);
    parseAndBinUpdate(receiveBuffer);
  }
}

// --- 持久化存储初始化 ---
void initPersistence() {
  if (!prefs.begin(PREFS_NAMESPACE, false)) {  // false=读写模式
    Serial.println(F("❌ 持久化存储初始化失败！"));
    while (1);
  }
  Serial.println(F("✅ 持久化存储初始化成功"));
}

// --- 从Flash加载仓位数据 ---
void loadBinsFromFlash() {
  binCount = prefs.getUInt(PREFS_BIN_COUNT, 0);  // 读取仓位数量，默认0
  if (binCount > MAX_BINS) binCount = MAX_BINS;  // 防溢出

  for (uint8_t i = 0; i < binCount; i++) {
    String key = PREFS_BIN_PREFIX + String(i);
    // 读取单条仓位数据（4个字段：id(1)+weight(4)+item(4)+lastItem(4) = 13字节）
    uint8_t data[13] = {0};
    size_t len = prefs.getBytes(key.c_str(), data, 13);  // 增加 .c_str() 转换
    if (len != 13) {
      bins[i] = {0, 0.0, 0, 0};  // 读取失败则初始化
      continue;
    }
    // 解析二进制数据
    bins[i].id = data[0];
    memcpy(&bins[i].weight, &data[1], 4);
    memcpy(&bins[i].item, &data[5], 4);
    memcpy(&bins[i].lastItem, &data[9], 4);
  }

  Serial.printf("✅ 从Flash加载 %u 个仓位数据\n", binCount);
  for (uint8_t i = 0; i < binCount; i++) {
    Serial.printf("  仓位%u: ID=%u, 重量=%.2f, 数量=%lu\n", i+1, bins[i].id, bins[i].weight, bins[i].item);
  }
}

// --- 保存仓位数据到Flash（实时调用，确保数据不丢失）---
void saveBinsToFlash() {
  prefs.putUInt(PREFS_BIN_COUNT, binCount);  // 先保存仓位数量

  for (uint8_t i = 0; i < binCount; i++) {
    String key = PREFS_BIN_PREFIX + String(i);
    // 打包二进制数据（减少Flash写入次数，提升寿命）
    uint8_t data[13];
    data[0] = bins[i].id;
    memcpy(&data[1], &bins[i].weight, 4);
    memcpy(&data[5], &bins[i].item, 4);
    memcpy(&data[9], &bins[i].lastItem, 4);
    prefs.putBytes(key.c_str(), data, 13);  // 增加 .c_str() 转换
  }

  Serial.printf("💾 仓位数据已保存到Flash，当前共 %u 个仓位\n", binCount);
}

// --- Wi-Fi 初始化 ---
void initWiFi() {
  Serial.print("📡 正在连接 Wi-Fi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("✅ Wi-Fi 连接成功!");
  Serial.print("📲 请访问网页: http://") ;
  Serial.println(WiFi.localIP());
}

// --- 解析并更新仓位数据 ---
void parseAndBinUpdate(char* data) {
  uint8_t id = 0;
  float weight = 0.0;
  uint32_t item = 0;

  char* idPtr = strstr(data, "id:");
  if (idPtr != NULL) id = (uint8_t)atoi(idPtr + 3);

  char* weightPtr = strstr(data, "weight:");
  if (weightPtr != NULL) weight = atof(weightPtr + 7);

  char* itemPtr = strstr(data, "item:");
  if (itemPtr != NULL) item = atol(itemPtr + 5);

  int index = findBinIndexById(id);
  if (index < 0) {
    if (binCount < MAX_BINS) {
      index = binCount;
      bins[index].id = id;
      bins[index].weight = weight;
      bins[index].item = item;
      bins[index].lastItem = item;
      binCount++;
      saveBinsToFlash();  // 新增仓位，立即保存
    } else {
      Serial.printf("⚠️  仓位已满（最大%d个），丢弃ID=%u的数据\n", MAX_BINS, id);
      return;
    }
  } else {
    BinInfo& b = bins[index];
    b.weight = weight;
    int32_t delta = (int32_t)item - (int32_t)b.lastItem;
    if (delta != 0) {
      addLog(b.id, delta, item);
      b.lastItem = item;
    }
    b.item = item;
    // 实时数据更新无需频繁保存Flash，避免磨损
  }

  Serial.printf("📊 更新仓位 %u: weight=%.2f, item=%lu\r\n", id, weight, item);
}

// --- 查找仓位索引 ---
int findBinIndexById(uint8_t id) {
  for (int i = 0; i < binCount; i++) {
    if (bins[i].id == id) return i;
  }
  return -1;
}

// --- 添加日志（循环队列） ---
void addLog(uint8_t binId, int32_t delta, uint32_t itemNew) {
  uint32_t nowSec = millis() / 1000;
  logs[logHead].binId  = binId;
  logs[logHead].delta  = delta;
  logs[logHead].itemNew= itemNew;
  logs[logHead].time   = nowSec;

  logHead = (logHead + 1) % MAX_LOGS;
  if (logHead == logTail) {
    logTail = (logTail + 1) % MAX_LOGS;
  }
}

// --- 生成所有仓位的 JSON ---
String getBinsJSON() {
  String json = "[";
  for (int i = 0; i < binCount; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"id\":" + String(bins[i].id) + ",";
    json += "\"weight\":" + String(bins[i].weight, 2) + ",";  // 保留2位小数
    json += "\"item\":" + String(bins[i].item);
    json += "}";
  }
  json += "]";
  return json;
}

// --- 生成某个仓位的日志 JSON ---
String getLogsJSON(uint8_t binId) {
  String json = "[";
  bool first = true;

  for (uint32_t i = logTail; i != logHead; i = (i + 1) % MAX_LOGS) {
    const LogEntry& e = logs[i];
    if (e.binId != binId) continue;

    if (!first) json += ",";
    first = false;

    json += "{";
    json += "\"delta\":" + String(e.delta) + ",";
    json += "\"itemNew\":" + String(e.itemNew) + ",";
    json += "\"time\":" + String(e.time);
    json += "}";
  }
  json += "]";
  return json;
}

// --- WebServer 路由设置（新增编辑仓位路由）---
void setupWebServer() {
  // 1. 主页（HTML，布局优化+编辑功能）
  server.on("/", []() {
    server.send(200, "text/html", getWebPageHTML());
  });

  // 2. 获取所有仓位数据 (GET)
  server.on("/bins", []() {
    server.send(200, "application/json", getBinsJSON());
  });

  // 3. 获取某个仓位的日志 ?id=1 (GET)
  server.on("/logs", []() {
    if (!server.hasArg("id")) {
      server.send(400, "text/plain", "Missing id");
      return;
    }
    uint8_t id = (uint8_t)server.arg("id").toInt();
    server.send(200, "application/json", getLogsJSON(id));
  });

  // 4. 添加仓位 (POST /bins/add)
  server.on("/bins/add", HTTP_POST, []() {
    if (!server.hasArg("id")) {
      server.send(400, "text/plain", "Missing id");
      return;
    }
    uint8_t id = (uint8_t)server.arg("id").toInt();
    if (id < 1 || id > 254) {  // 增加ID合法性校验
      server.send(400, "text/plain", "ID must 1-254");
      return;
    }
    if (findBinIndexById(id) >= 0) {
      server.send(400, "text/plain", "Bin already exists");
      return;
    }
    if (binCount >= MAX_BINS) {
      server.send(500, "text/plain", "Max bins reached(" + String(MAX_BINS) + ")");
      return;
    }

    bins[binCount].id = id;
    bins[binCount].weight = 0;
    bins[binCount].item = 0;
    bins[binCount].lastItem = 0;
    binCount++;
    saveBinsToFlash();  // 新增后立即保存

    Serial.printf("🌐 Web操作: 添加仓位 ID=%u\n", id);
    server.send(200, "text/plain", "OK");
  });

  // 5. 删除仓位 (POST /bins/delete)
  server.on("/bins/delete", HTTP_POST, []() {
    if (!server.hasArg("id")) {
      server.send(400, "text/plain", "Missing id");
      return;
    }
    uint8_t id = (uint8_t)server.arg("id").toInt();
    int idx = findBinIndexById(id);
    if (idx < 0) {
      server.send(404, "text/plain", "Bin not found");
      return;
    }
    // 移动数据，删除指定仓位
    for (int i = idx; i < binCount - 1; i++) {
      bins[i] = bins[i + 1];
    }
    binCount--;
    saveBinsToFlash();  // 删除后立即保存

    Serial.printf("🌐 Web操作: 删除仓位 ID=%u\n", id);
    server.send(200, "text/plain", "OK");
  });

  // 6. 编辑仓位编号（新增核心路由）
  server.on("/bins/edit", HTTP_POST, []() {
    if (!server.hasArg("oldId") || !server.hasArg("newId")) {
      server.send(400, "text/plain", "Missing oldId/newId");
      return;
    }
    uint8_t oldId = (uint8_t)server.arg("oldId").toInt();
    uint8_t newId = (uint8_t)server.arg("newId").toInt();

    // 合法性校验
    if (newId < 1 || newId > 254) {
      server.send(400, "text/plain", "New ID must 1-254");
      return;
    }
    int idx = findBinIndexById(oldId);
    if (idx < 0) {
      server.send(404, "text/plain", "Old bin not found");
      return;
    }
    if (findBinIndexById(newId) >= 0 && oldId != newId) {
      server.send(400, "text/plain", "New ID already exists");
      return;
    }

    // 修改编号
    bins[idx].id = newId;
    saveBinsToFlash();  // 编辑后立即保存

    Serial.printf("🌐 Web操作: 编辑仓位 ID=%u -> %u\n", oldId, newId);
    server.send(200, "text/plain", "OK");
  });

  server.begin();
}

// --- 网页 HTML（优化布局+新增编辑功能+友好提示）---
String getWebPageHTML() {
  String html = R"=====(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>ESP32 仓位实时管理系统</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: "Microsoft YaHei", Arial, sans-serif;
      background-color: #f5f7fa;
      color: #333;
      padding: 16px;
      max-width: 800px;
      margin: 0 auto;
    }
    .page-title {
      text-align: center;
      font-size: 22px;
      font-weight: 600;
      color: #2c3e50;
      margin-bottom: 20px;
    }
    .toolbar {
      text-align: center;
      margin-bottom: 20px;
    }
    .btn {
      border: none;
      border-radius: 8px;
      padding: 8px 16px;
      margin: 0 6px;
      cursor: pointer;
      font-size: 14px;
      font-weight: 500;
      transition: opacity 0.2s;
    }
    .btn:hover { opacity: 0.9; }
    .btn-primary { background: #3498db; color: #fff; }
    .btn-danger { background: #e74c3c; color: #fff; }
    .btn-edit { background: #f39c12; color: #fff; padding: 4px 8px; font-size: 12px; }
    .panel {
      background: #fff;
      border-radius: 10px;
      padding: 16px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.08);
      margin-bottom: 20px;
      display: none;
    }
    .panel.open { display: block; }
    .panel-title {
      font-size: 16px;
      font-weight: 600;
      margin-bottom: 12px;
      color: #34495e;
    }
    .form-item {
      margin-bottom: 12px;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .form-item label {
      font-size: 14px;
      color: #666;
      width: 100px;
      text-align: right;
    }
    .form-item input {
      flex: 1;
      max-width: 150px;
      padding: 6px 10px;
      border: 1px solid #ddd;
      border-radius: 6px;
      font-size: 14px;
    }
    .bins-container {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
      gap: 16px;
      margin-bottom: 20px;
    }
    .bin-card {
      background: #fff;
      border-radius: 10px;
      padding: 16px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.08);
    }
    .bin-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 12px;
      padding-bottom: 8px;
      border-bottom: 1px solid #eee;
    }
    .bin-id {
      font-size: 18px;
      font-weight: 600;
      color: #2c3e50;
    }
    .bin-body {
      font-size: 14px;
      line-height: 1.8;
    }
    .bin-item {
      display: flex;
      justify-content: space-between;
      margin-bottom: 4px;
    }
    .bin-label { color: #666; }
    .bin-value { color: #2c3e50; font-weight: 500; }
    .weight-val { color: #3498db; }
    .item-val { color: #9b59b6; }
    .bin-actions {
      margin-top: 12px;
      display: flex;
      justify-content: flex-end;
      gap: 8px;
    }
    .bin-logs {
      margin-top: 12px;
      padding-top: 12px;
      border-top: 1px solid #eee;
      font-size: 12px;
      max-height: 200px;
      overflow-y: auto;
      display: none;
    }
    .bin-logs.open { display: block; }
    .log-entry {
      padding: 4px 0;
      border-bottom: 1px solid #f5f5f5;
    }
    .log-delta { font-weight: 600; }
    .log-delta.plus { color: #27ae60; }
    .log-delta.minus { color: #e74c3c; }
    .status-bar {
      text-align: center;
      font-size: 12px;
      color: #999;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
    }
    .dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: #3498db;
      animation: pulse 2s infinite;
    }
    @keyframes pulse {
      0% { opacity: 1; }
      50% { opacity: 0.3; }
      100% { opacity: 1; }
    }
    .toast {
      position: fixed;
      top: 20px;
      left: 50%;
      transform: translateX(-50%);
      padding: 10px 20px;
      border-radius: 8px;
      color: #fff;
      font-size: 14px;
      z-index: 999;
      display: none;
    }
    .toast.success { background: #2ecc71; }
    .toast.error { background: #e74c3c; }
  </style>
</head>
<body>
  <h1 class="page-title">📦 ESP32 仓位实时管理系统</h1>

  <div class="toolbar">
    <button class="btn btn-primary" onclick="toggleAddPanel()">添加仓位</button>
  </div>

  <!-- 添加仓位面板 -->
  <div class="panel" id="addPanel">
    <div class="panel-title">添加新仓位</div>
    <div class="form-item">
      <label>仓位编号：</label>
      <input type="number" id="addId" min="1" max="254" placeholder="请输入1-254">
      <button class="btn btn-primary" onclick="addBin()">确认添加</button>
    </div>
  </div>

  <!-- 仓位卡片容器 -->
  <div class="bins-container" id="binsContainer">
    <!-- 动态生成仓位卡片 -->
  </div>

  <!-- 状态栏 -->
  <div class="status-bar">
    <span class="dot"></span>
    实时监控中 | 数据断电持久化
  </div>

  <!-- 提示弹窗 -->
  <div class="toast" id="toast"></div>

  <script>
    let binsData = [];
    let binElements = {}; // 缓存仓位DOM元素，key: binId
    const toast = document.getElementById('toast');
    const refreshInterval = 500; // 数据刷新间隔(ms)

    // 显示提示框
    function showToast(text, isSuccess = true) {
      toast.textContent = text;
      toast.className = 'toast ' + (isSuccess ? 'success' : 'error');
      toast.style.display = 'block';
      setTimeout(() => {
        toast.style.display = 'none';
      }, 2000);
    }

    // 获取仓位数据
    function fetchBins() {
      fetch('/bins')
        .then(r => {
          if (!r.ok) throw new Error('Network error');
          return r.json();
        })
        .then(arr => {
          binsData = arr;
          updateBinsDOM();
        })
        .catch(e => console.error('获取仓位失败：', e));
    }

    // 格式化时间戳（秒）
    function formatTime(sec) {
      const d = new Date(sec * 1000);
      return [
        d.getHours().toString().padStart(2, '0'),
        d.getMinutes().toString().padStart(2, '0'),
        d.getSeconds().toString().padStart(2, '0')
      ].join(':');
    }

    // 获取仓位日志
    function fetchLogs(binId, cardEl) {
      fetch(`/logs?id=${binId}`)
        .then(r => r.json())
        .then(logs => {
          const logEl = cardEl.querySelector('.bin-logs');
          if (!logs || logs.length === 0) {
            logEl.innerHTML = '<div style="text-align: center; color: #999; padding: 10px 0;">暂无操作日志</div>';
            return;
          }
          logEl.innerHTML = logs.map(l => {
            const deltaClass = l.delta > 0 ? 'plus' : l.delta < 0 ? 'minus' : '';
            const deltaText = l.delta > 0 ? `放入 +${l.delta}` : l.delta < 0 ? `拿走 ${l.delta}` : '无变化';
            return `
              <div class="log-entry">
                <span class="log-delta ${deltaClass}">${deltaText}</span> | 
                当前数量：${l.itemNew} | 
                时间：${formatTime(l.time)}
              </div>
            `;
          }).join('');
        })
        .catch(e => {
          console.error('获取日志失败：', e);
          cardEl.querySelector('.bin-logs').innerHTML = '<div style="color: #e74c3c; padding: 10px 0;">日志加载失败</div>';
        });
    }

    // 切换日志显示/隐藏
    function toggleLogs(cardEl) {
      const logEl = cardEl.querySelector('.bin-logs');
      logEl.classList.toggle('open');
      if (logEl.classList.contains('open')) {
        const binId = cardEl.dataset.id;
        fetchLogs(binId, cardEl);
      }
    }

    // 切换添加仓位面板
    function toggleAddPanel() {
      const panel = document.getElementById('addPanel');
      panel.classList.toggle('open');
      if (panel.classList.contains('open')) {
        document.getElementById('addId').focus();
      }
    }

    // 更新仓位DOM（增量更新，避免重复渲染）
    function updateBinsDOM() {
      const container = document.getElementById('binsContainer');
      const presentIds = new Set(binsData.map(b => b.id));

      // 删除已不存在的仓位卡片
      Object.keys(binElements).forEach(binId => {
        if (!presentIds.has(Number(binId))) {
          binElements[binId].remove();
          delete binElements[binId];
        }
      });

      // 更新或创建仓位卡片
      binsData.forEach(bin => {
        let cardEl = binElements[bin.id];
        if (!cardEl) {
          // 创建新卡片
          cardEl = document.createElement('div');
          cardEl.className = 'bin-card';
          cardEl.dataset.id = bin.id;
          cardEl.innerHTML = `
            <div class="bin-header">
              <div class="bin-id">仓位 #${bin.id}</div>
              <button class="btn btn-edit" onclick="editBinId(${bin.id})">编辑编号</button>
            </div>
            <div class="bin-body">
              <div class="bin-item">
                <span class="bin-label">当前重量：</span>
                <span class="bin-value weight-val">${bin.weight.toFixed(2)}</span> g
              </div>
              <div class="bin-item">
                <span class="bin-label">当前数量：</span>
                <span class="bin-value item-val">${bin.item}</span> pcs
              </div>
            </div>
            <div class="bin-actions">
              <button class="btn btn-primary" onclick="toggleLogs(this.parentElement.parentElement)">查看日志</button>
              <button class="btn btn-danger" onclick="deleteBin(${bin.id})">删除</button>
            </div>
            <div class="bin-logs"></div>
          `;
          container.appendChild(cardEl);
          binElements[bin.id] = cardEl;
        } else {
          // 增量更新数值，不重建卡片
          cardEl.querySelector('.weight-val').textContent = bin.weight.toFixed(2);
          cardEl.querySelector('.item-val').textContent = bin.item;
        }
      });

      // 无仓位时显示提示
      if (binsData.length === 0) {
        container.innerHTML = '<div style="text-align: center; color: #999; padding: 40px 0; grid-column: 1/-1;">暂无仓位，点击上方「添加仓位」创建</div>';
        binElements = {};
      }
    }

    // 添加仓位
    function addBin() {
      const input = document.getElementById('addId');
      const id = parseInt(input.value.trim());
      if (isNaN(id) || id < 1 || id > 254) {
        showToast('请输入有效的编号（1-254）', false);
        input.focus();
        return;
      }

      fetch(`/bins/add?id=${id}`, { method: 'POST' })
        .then(r => r.text())
        .then(res => {
          if (res === 'OK') {
            showToast('仓位添加成功！');
            input.value = '';
            fetchBins();
            toggleAddPanel(); // 关闭面板
          } else {
            showToast(`添加失败：${res}`, false);
          }
        })
        .catch(e => {
          showToast('网络请求失败', false);
          console.error(e);
        });
    }

    // 删除仓位
    function deleteBin(binId) {
      if (!confirm(`确定要删除仓位 #${binId} 吗？\n删除后数据将无法恢复！`)) return;

      fetch(`/bins/delete?id=${binId}`, { method: 'POST' })
        .then(r => r.text())
        .then(res => {
          if (res === 'OK') {
            showToast('仓位删除成功！');
            fetchBins();
          } else {
            showToast(`删除失败：${res}`, false);
          }
        })
        .catch(e => {
          showToast('网络请求失败', false);
          console.error(e);
        });
    }

    // 编辑仓位编号
    function editBinId(oldId) {
      const newId = prompt(`请输入新的仓位编号（1-254）\n当前编号：${oldId}`, oldId);
      if (newId === null) return; // 取消操作
      const newIdNum = parseInt(newId.trim());
      if (isNaN(newIdNum) || newIdNum < 1 || newIdNum > 254) {
        showToast('请输入有效的编号（1-254）', false);
        return;
      }
      if (newIdNum === oldId) return; // 编号未变

      fetch(`/bins/edit?oldId=${oldId}&newId=${newIdNum}`, { method: 'POST' })
        .then(r => r.text())
        .then(res => {
          if (res === 'OK') {
            showToast(`编号修改成功：${oldId} → ${newIdNum}`);
            fetchBins();
          } else {
            showToast(`修改失败：${res}`, false);
          }
        })
        .catch(e => {
          showToast('网络请求失败', false);
          console.error(e);
        });
    }

    // 回车键触发添加仓位
    document.getElementById('addId').addEventListener('keydown', e => {
      if (e.key === 'Enter') addBin();
    });

    // 定时刷新数据
    setInterval(fetchBins, refreshInterval);

    // 首次加载
    fetchBins();
  </script>
</body>
</html>
  )=====";
  return html;
}