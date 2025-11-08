# ESP8266 MQTT继电器控制系统

基于ESP8266和Arduino的四路Modbus RTU继电器控制系统，支持通过MQTT服务器远程控制继电器开关。

## 功能特性

- ✅ 四路Modbus RTU继电器控制
- ✅ MQTT远程控制
- ✅ WiFi自动配网（WiFiManager）
- ✅ 手动清除配网信息（长按Flash按钮5秒）
- ✅ MQTT自动重连保持连接
- ✅ 心跳机制（30秒间隔）
- ✅ 状态发布

## 硬件要求

- ESP8266开发板（NodeMCU、WeMos D1等）
- Modbus RTU 4路继电器模块
- 连接线

## 接线说明

### Modbus继电器模块连接
- **Modbus模块 RX** → **ESP8266 GPIO4** (D2)
- **Modbus模块 TX** → **ESP8266 GPIO5** (D1)
- **Modbus模块 GND** → **ESP8266 GND**
- **Modbus模块 VCC** → **ESP8266 5V** (或3.3V，根据模块要求)

### 清除配网按钮
- **Flash按钮 (GPIO0)** - 长按5秒清除WiFi配网信息

> **注意**：如果您的Modbus模块需要连接到硬件Serial（GPIO1/TX, GPIO3/RX），需要：
> 1. 注释掉所有`Serial.print`调试输出
> 2. 修改`sendModbusCommand`函数，使用`Serial.write`替代`modbusSerial.write`

## 依赖库

在Arduino IDE中安装以下库：

1. **ESP8266WiFi** (ESP8266核心库自带)
2. **PubSubClient** - MQTT客户端库
   - 安装方法：工具 → 管理库 → 搜索 "PubSubClient" → 安装
3. **WiFiManager** - WiFi配网库
   - 安装方法：工具 → 管理库 → 搜索 "WiFiManager" → 安装
4. **ArduinoJson** - JSON解析库
   - 安装方法：工具 → 管理库 → 搜索 "ArduinoJson" → 安装
5. **SoftwareSerial** (ESP8266核心库自带)

## 配置说明

### MQTT服务器配置
代码中的MQTT服务器配置：
```cpp
#define MQTT_SERVER "39.101.179.153"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "ESP8266_Relay_Controller"
```

### MQTT话题
- **控制话题**: `relay/control` - 接收控制命令
- **状态话题**: `relay/status` - 发布继电器状态
- **心跳话题**: `relay/heartbeat` - 发布心跳信息

## 使用方法

### 1. 上传程序
1. 打开Arduino IDE
2. 选择开发板：工具 → 开发板 → NodeMCU 1.0 (ESP-12E Module)
3. 选择正确的端口
4. 上传程序到ESP8266

### 2. WiFi配网
首次使用或清除配网后：
1. ESP8266会创建一个WiFi热点：`ESP8266_Relay_Config`
2. 密码：`12345678`
3. 连接该热点后，浏览器会自动打开配网页面（或手动访问 http://192.168.4.1）
4. 输入您的WiFi名称和密码
5. 保存后设备会自动重启并连接WiFi

### 3. 清除配网信息
长按ESP8266开发板上的**Flash按钮（GPIO0）**5秒，系统会清除WiFi配置并重启进入配网模式。

### 4. MQTT控制命令

#### 控制继电器
```json
{
  "action": "control",
  "relay": "relay1",
  "state": "on"
}
```

支持的继电器：
- `relay1` - 1号继电器
- `relay2` - 2号继电器
- `relay3` - 3号继电器
- `relay4` - 4号继电器

支持的状态：
- `on` - 开启
- `off` - 关闭

#### 查询状态
```json
{
  "action": "status"
}
```

## Modbus RTU继电器控制命令

系统使用以下Modbus RTU命令控制继电器：

### 1号继电器
- **开启**: `01 05 00 00 FF 00 8C 3A`
- **关闭**: `01 05 00 00 00 00 CD CA`

### 2号继电器
- **开启**: `01 05 00 01 FF 00 DD FA`
- **关闭**: `01 05 00 01 00 00 9C 0A`

### 3号继电器
- **开启**: `01 05 00 02 FF 00 2D FA`
- **关闭**: `01 05 00 02 00 00 6C 0A`

### 4号继电器
- **开启**: `01 05 00 03 FF 00 7C 3A`
- **关闭**: `01 05 00 03 00 00 3D CA`

## 状态信息

系统会定期发送状态信息到MQTT主题：

### 状态消息格式
```json
{
  "action": "status",
  "system": "online",
  "wifi": "connected",
  "mqtt": "connected",
  "ip": "192.168.1.100",
  "rssi": -45,
  "relays": {
    "relay1": "unknown",
    "relay2": "unknown",
    "relay3": "unknown",
    "relay4": "unknown"
  },
  "timestamp": 1234567890
}
```

### 心跳消息格式
```json
{
  "type": "heartbeat",
  "client_id": "ESP8266_Relay_Controller",
  "status": "alive",
  "wifi": "connected",
  "mqtt": "connected",
  "uptime": 123456,
  "timestamp": 1234567890
}
```

## 故障排除

### 1. WiFi连接失败
- 检查WiFi名称和密码是否正确
- 确认WiFi信号强度足够
- 清除配网信息后重新配置

### 2. MQTT连接失败
- 检查MQTT服务器地址和端口是否正确
- 确认网络连接正常
- 检查MQTT服务器是否可访问

### 3. 继电器无响应
- 检查Modbus模块接线是否正确
- 确认波特率设置为9600
- 检查Modbus模块供电是否正常
- 使用串口监视器查看是否有Modbus命令发送

### 4. 串口通信问题
- 如果使用SoftwareSerial，确保GPIO引脚连接正确
- 如果使用硬件Serial，需要注释掉所有调试输出

## 调试

打开Arduino IDE的串口监视器（波特率115200）可以查看：
- 系统启动信息
- WiFi连接状态
- MQTT连接状态
- 接收到的MQTT命令
- 发送的Modbus命令
- 错误信息

## 注意事项

1. **Serial通信**：默认使用SoftwareSerial（GPIO4/GPIO5）与Modbus模块通信，Serial用于调试输出
2. **MQTT保持连接**：系统会自动重连MQTT，保持连接稳定
3. **心跳机制**：每30秒发送一次心跳，确保服务器知道设备在线
4. **清除配网**：长按Flash按钮5秒清除配网，设备会重启进入配网模式

## 许可证

MIT License

