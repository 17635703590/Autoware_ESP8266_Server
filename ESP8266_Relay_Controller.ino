/*
 * ESP8266 MQTT继电器控制系统
 * 通过MQTT服务器控制四路Modbus RTU继电器模块
 * 支持WiFi配网、清除配网信息、MQTT自动重连
 * 
 * 继电器模块：Modbus RTU 4路继电器
 * 通信方式：使用SoftwareSerial与Modbus模块通信
 * Modbus模块连接：RX=GPIO4, TX=GPIO5 (可自定义)
 * 波特率：9600
 * 
 * 注意：如果使用硬件Serial（GPIO1/TX, GPIO3/RX）与Modbus模块通信，
 *       需要注释掉所有Serial.print调试输出，并修改sendModbusCommand函数
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>

// ==================== 配置参数 ====================
#define MQTT_SERVER "39.101.179.153"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "ESP8266_Relay_Controller"
#define MQTT_TOPIC_CONTROL "relay/control"
#define MQTT_TOPIC_STATUS "relay/status"
#define MQTT_TOPIC_HEARTBEAT "relay/heartbeat"

#define MODBUS_RX_PIN 4           // Modbus模块RX引脚连接ESP8266的GPIO4
#define MODBUS_TX_PIN 5           // Modbus模块TX引脚连接ESP8266的GPIO5
#define MODBUS_BAUD 9600          // Modbus RTU 波特率
#define RESET_PIN 0               // GPIO0 - 清除配网引脚（Flash按钮）
#define RESET_HOLD_TIME 5000      // 长按5秒清除配网

#define HEARTBEAT_INTERVAL 30000  // 心跳间隔30秒
#define MQTT_RECONNECT_DELAY 5000 // MQTT重连延迟5秒

// 创建SoftwareSerial对象用于Modbus通信
SoftwareSerial modbusSerial(MODBUS_RX_PIN, MODBUS_TX_PIN); // RX, TX

// ==================== Modbus RTU 继电器控制命令 ====================
// 1号继电器
const uint8_t RELAY1_ON[] = {0x01, 0x05, 0x00, 0x00, 0xFF, 0x00, 0x8C, 0x3A};
const uint8_t RELAY1_OFF[] = {0x01, 0x05, 0x00, 0x00, 0x00, 0x00, 0xCD, 0xCA};

// 2号继电器
const uint8_t RELAY2_ON[] = {0x01, 0x05, 0x00, 0x01, 0xFF, 0x00, 0xDD, 0xFA};
const uint8_t RELAY2_OFF[] = {0x01, 0x05, 0x00, 0x01, 0x00, 0x00, 0x9C, 0x0A};

// 3号继电器
const uint8_t RELAY3_ON[] = {0x01, 0x05, 0x00, 0x02, 0xFF, 0x00, 0x2D, 0xFA};
const uint8_t RELAY3_OFF[] = {0x01, 0x05, 0x00, 0x02, 0x00, 0x00, 0x6C, 0x0A};

// 4号继电器
const uint8_t RELAY4_ON[] = {0x01, 0x05, 0x00, 0x03, 0xFF, 0x00, 0x7C, 0x3A};
const uint8_t RELAY4_OFF[] = {0x01, 0x05, 0x00, 0x03, 0x00, 0x00, 0x3D, 0xCA};

// ==================== 全局变量 ====================
WiFiClient espClient;
PubSubClient mqtt(espClient);

unsigned long lastHeartbeat = 0;
unsigned long lastReconnectAttempt = 0;
unsigned long resetButtonPressTime = 0;
bool resetButtonPressed = false;

// ==================== 函数声明 ====================
void setupWiFi();
void clearWiFiConfig();
void checkResetButton();
void setupMQTT();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void controlRelay(int relayNum, bool state);
void publishStatus();
void publishHeartbeat();
void sendModbusCommand(const uint8_t* command, int length);

// ==================== 初始化 ====================
void setup() {
  // 初始化调试串口（用于USB调试）
  Serial.begin(115200);
  Serial.println("\n\n==========================================");
  Serial.println("ESP8266 MQTT继电器控制系统启动");
  Serial.println("==========================================");
  
  // 初始化Modbus串口
  modbusSerial.begin(MODBUS_BAUD);
  Serial.print("Modbus串口初始化完成，波特率: ");
  Serial.println(MODBUS_BAUD);
  
  // 初始化复位引脚（GPIO0）
  pinMode(RESET_PIN, INPUT_PULLUP);
  
  // 检查是否需要清除配网
  checkResetButton();
  
  // WiFi连接
  setupWiFi();
  
  // MQTT设置
  setupMQTT();
  
  Serial.println("系统初始化完成，等待MQTT命令...");
  Serial.println("支持的命令格式:");
  Serial.println("{\"action\": \"control\", \"relay\": \"relay1\", \"state\": \"on\"}");
  Serial.println("{\"action\": \"control\", \"relay\": \"relay1\", \"state\": \"off\"}");
  Serial.println("{\"action\": \"status\"}");
  Serial.println("==========================================\n");
  
  // 发布初始状态
  publishStatus();
}

// ==================== 主循环 ====================
void loop() {
  // 检查复位按钮
  checkResetButton();
  
  // 检查并保持MQTT连接
  if (!mqtt.connected()) {
    reconnectMQTT();
  } else {
    mqtt.loop();
  }
  
  // 发送心跳
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    publishHeartbeat();
    lastHeartbeat = millis();
  }
  
  delay(100);
}

// ==================== WiFi配置 ====================
void setupWiFi() {
  Serial.println("正在连接WiFi...");
  
  WiFiManager wifiManager;
  
  // 设置配网超时时间（3分钟）
  wifiManager.setConfigPortalTimeout(180);
  
  // 自动连接WiFi，如果连接失败则启动配网模式
  if (!wifiManager.autoConnect("ESP8266_Relay_Config", "12345678")) {
    Serial.println("WiFi配网超时，系统将重启...");
    delay(3000);
    ESP.restart();
  }
  
  Serial.println("WiFi连接成功!");
  Serial.print("IP地址: ");
  Serial.println(WiFi.localIP());
  Serial.print("信号强度: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
}

// ==================== 清除WiFi配置 ====================
void clearWiFiConfig() {
  Serial.println("\n检测到清除配网操作...");
  Serial.println("正在清除WiFi配置...");
  
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  
  Serial.println("WiFi配置已清除，系统将重启进入配网模式...");
  delay(2000);
  ESP.restart();
}

// ==================== 检查复位按钮 ====================
void checkResetButton() {
  int buttonState = digitalRead(RESET_PIN);
  
  if (buttonState == LOW) {  // 按钮按下（GPIO0内部上拉，按下为LOW）
    if (!resetButtonPressed) {
      resetButtonPressTime = millis();
      resetButtonPressed = true;
      Serial.println("检测到复位按钮按下，请保持按住5秒以清除配网...");
    } else {
      // 检查是否长按超过5秒
      if (millis() - resetButtonPressTime > RESET_HOLD_TIME) {
        clearWiFiConfig();
        resetButtonPressed = false;
      }
    }
  } else {
    if (resetButtonPressed) {
      unsigned long pressDuration = millis() - resetButtonPressTime;
      if (pressDuration < RESET_HOLD_TIME) {
        Serial.println("按钮释放，未达到清除配网时间要求");
      }
      resetButtonPressed = false;
    }
  }
}

// ==================== MQTT设置 ====================
void setupMQTT() {
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  reconnectMQTT();
}

// ==================== MQTT重连 ====================
void reconnectMQTT() {
  unsigned long now = millis();
  
  // 限制重连频率
  if (now - lastReconnectAttempt < MQTT_RECONNECT_DELAY) {
    return;
  }
  lastReconnectAttempt = now;
  
  // 检查WiFi连接
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi未连接，尝试重连...");
    WiFi.reconnect();
    delay(1000);
    return;
  }
  
  Serial.print("正在连接MQTT服务器: ");
  Serial.print(MQTT_SERVER);
  Serial.print(":");
  Serial.println(MQTT_PORT);
  
  // 尝试连接MQTT
  if (mqtt.connect(MQTT_CLIENT_ID)) {
    Serial.println("MQTT连接成功!");
    
    // 订阅主题
    mqtt.subscribe(MQTT_TOPIC_CONTROL);
    Serial.print("已订阅主题: ");
    Serial.println(MQTT_TOPIC_CONTROL);
    
    // 发布在线状态
    publishStatus();
  } else {
    Serial.print("MQTT连接失败，错误代码: ");
    Serial.println(mqtt.state());
    Serial.println("将在5秒后重试...");
  }
}

// ==================== MQTT消息回调 ====================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // 将payload转换为字符串
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  
  Serial.print("收到MQTT消息 [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);
  
  // 解析JSON消息
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.print("JSON解析失败: ");
    Serial.println(error.c_str());
    return;
  }
  
  String action = doc["action"].as<String>();
  
  if (action == "control") {
    String relay = doc["relay"].as<String>();
    String state = doc["state"].as<String>();
    
    Serial.print("控制命令: ");
    Serial.print(relay);
    Serial.print(" -> ");
    Serial.println(state);
    
    // 确定继电器编号
    int relayNum = 0;
    if (relay == "relay1") relayNum = 1;
    else if (relay == "relay2") relayNum = 2;
    else if (relay == "relay3") relayNum = 3;
    else if (relay == "relay4") relayNum = 4;
    else {
      Serial.println("无效的继电器编号");
      return;
    }
    
    // 控制继电器
    bool relayState = (state == "on");
    controlRelay(relayNum, relayState);
    
    // 发布状态更新
    publishStatus();
    
  } else if (action == "status") {
    Serial.println("查询继电器状态");
    publishStatus();
  } else {
    Serial.print("未知的操作: ");
    Serial.println(action);
  }
}

// ==================== 控制继电器 ====================
void controlRelay(int relayNum, bool state) {
  const uint8_t* command = NULL;
  int commandLength = 8;
  
  // 根据继电器编号和状态选择命令
  switch (relayNum) {
    case 1:
      command = state ? RELAY1_ON : RELAY1_OFF;
      break;
    case 2:
      command = state ? RELAY2_ON : RELAY2_OFF;
      break;
    case 3:
      command = state ? RELAY3_ON : RELAY3_OFF;
      break;
    case 4:
      command = state ? RELAY4_ON : RELAY4_OFF;
      break;
    default:
      Serial.println("无效的继电器编号");
      return;
  }
  
  Serial.print("控制继电器 ");
  Serial.print(relayNum);
  Serial.print(" -> ");
  Serial.println(state ? "开启" : "关闭");
  
  // 发送Modbus命令
  sendModbusCommand(command, commandLength);
  
  delay(50);  // 等待命令执行
}

// ==================== 发送Modbus命令 ====================
void sendModbusCommand(const uint8_t* command, int length) {
  Serial.print("发送Modbus命令: ");
  for (int i = 0; i < length; i++) {
    Serial.printf("%02X ", command[i]);
  }
  Serial.println();
  
  // 通过SoftwareSerial发送命令到Modbus RTU继电器模块
  modbusSerial.write(command, length);
  modbusSerial.flush();
  
  // 如果需要使用硬件Serial，取消下面的注释并注释掉上面的两行
  // Serial.write(command, length);
  // Serial.flush();
}

// ==================== 发布状态 ====================
void publishStatus() {
  if (!mqtt.connected()) {
    return;
  }
  
  StaticJsonDocument<512> doc;
  doc["action"] = "status";
  doc["system"] = "online";
  doc["wifi"] = WiFi.status() == WL_CONNECTED ? "connected" : "disconnected";
  doc["mqtt"] = mqtt.connected() ? "connected" : "disconnected";
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  doc["timestamp"] = millis();
  
  // 继电器状态（由于无法读取Modbus继电器状态，这里假设为未知）
  JsonObject relays = doc.createNestedObject("relays");
  relays["relay1"] = "unknown";
  relays["relay2"] = "unknown";
  relays["relay3"] = "unknown";
  relays["relay4"] = "unknown";
  
  char buffer[512];
  serializeJson(doc, buffer);
  
  mqtt.publish(MQTT_TOPIC_STATUS, buffer);
  Serial.println("状态已发布");
}

// ==================== 发布心跳 ====================
void publishHeartbeat() {
  if (!mqtt.connected()) {
    return;
  }
  
  StaticJsonDocument<256> doc;
  doc["type"] = "heartbeat";
  doc["client_id"] = MQTT_CLIENT_ID;
  doc["status"] = "alive";
  doc["wifi"] = WiFi.status() == WL_CONNECTED ? "connected" : "disconnected";
  doc["mqtt"] = mqtt.connected() ? "connected" : "disconnected";
  doc["uptime"] = millis();
  doc["timestamp"] = millis();
  
  char buffer[256];
  serializeJson(doc, buffer);
  
  mqtt.publish(MQTT_TOPIC_HEARTBEAT, buffer);
  Serial.println("心跳已发送");
}

