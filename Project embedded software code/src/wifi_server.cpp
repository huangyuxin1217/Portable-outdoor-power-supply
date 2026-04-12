#include "wifi_server.h"
#include "oled.h"
#include "stm32_comm.h"
#include <Adafruit_NeoPixel.h>

// 外部LED控制对象
extern Adafruit_NeoPixel strip;
extern int led_state;
extern int current_effect;
extern unsigned long effect_millis;
extern bool voice_recognition_enabled;
extern void playVoiceResponse(const char *filename);

// LED引脚定义
#define LED_PIN 2

// Web服务器对象
static WebServer server(80);

// 全局变量
static int counter = 0;
static int micLevel = 0;
static String receivedMessage = "";
static unsigned long messageDisplayTime = 0;
static bool showingMessage = false;

// LED颜色控制
static uint8_t led_r = 0, led_g = 255, led_b = 255;  // 默认青色

// 计算电池电量百分比（24V锂电池系统）
static int calculateBatteryPercentage(float voltage) {
    const float VOLTAGE_MAX = 25.2;  // 满电 (4.2V x 6)
    const float VOLTAGE_MIN = 18.0;  // 空电 (3.0V x 6)
    
    if (voltage >= VOLTAGE_MAX) return 100;
    if (voltage <= VOLTAGE_MIN) return 0;
    
    int percentage = (int)((voltage - VOLTAGE_MIN) / (VOLTAGE_MAX - VOLTAGE_MIN) * 100);
    return constrain(percentage, 0, 100);
}

// 处理根路径访问
static void handleRoot() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>ESP32 Voice Control</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }";
    html += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 15px; box-shadow: 0 5px 20px rgba(0,0,0,0.3); }";
    html += "h1 { color: #667eea; text-align: center; margin-bottom: 10px; }";
    html += "h2 { color: #555; font-size: 18px; border-bottom: 2px solid #667eea; padding-bottom: 5px; margin-top: 20px; }";
    html += ".info-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin: 15px 0; }";
    html += ".info-box { background: #f5f5f5; padding: 12px; border-radius: 8px; border-left: 4px solid #667eea; }";
    html += ".info-box strong { color: #667eea; display: block; font-size: 12px; }";
    html += ".info-box span { font-size: 18px; font-weight: bold; color: #333; }";
    html += ".led-status { text-align: center; font-size: 24px; padding: 15px; background: linear-gradient(90deg, #f093fb 0%, #f5576c 100%); color: white; border-radius: 10px; margin: 10px 0; }";
    html += ".btn-group { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin: 10px 0; }";
    html += "button { padding: 15px; background: #4CAF50; color: white; border: none; border-radius: 8px; cursor: pointer; font-size: 16px; font-weight: bold; transition: 0.3s; }";
    html += "button:hover { transform: scale(1.05); box-shadow: 0 5px 15px rgba(0,0,0,0.3); }";
    html += ".btn-off { background: #f44336; } .btn-off:hover { background: #d32f2f; }";
    html += ".btn-red { background: #e74c3c; } .btn-green { background: #2ecc71; } .btn-blue { background: #3498db; }";
    html += ".btn-yellow { background: #f1c40f; } .btn-purple { background: #9b59b6; } .btn-cyan { background: #1abc9c; }";
    html += "input[type='text'] { width: 100%; padding: 12px; margin: 10px 0; box-sizing: border-box; border: 2px solid #ddd; border-radius: 8px; font-size: 14px; }";
    html += ".status-bar { background: #e3f2fd; padding: 10px; border-radius: 8px; text-align: center; margin: 10px 0; }";
    html += ".color-slider-container { margin: 15px 0; padding: 15px; background: #f9f9f9; border-radius: 10px; }";
    html += ".color-preview { width: 100%; height: 50px; border-radius: 8px; margin: 10px 0; box-shadow: 0 2px 8px rgba(0,0,0,0.2); transition: 0.3s; }";
    html += "input[type='range'] { width: 100%; height: 8px; border-radius: 5px; background: linear-gradient(to right, red, yellow, lime, cyan, blue, magenta, red); outline: none; -webkit-appearance: none; }";
    html += "input[type='range']::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 25px; height: 25px; border-radius: 50%; background: white; cursor: pointer; box-shadow: 0 2px 5px rgba(0,0,0,0.3); }";
    html += "input[type='range']::-moz-range-thumb { width: 25px; height: 25px; border-radius: 50%; background: white; cursor: pointer; box-shadow: 0 2px 5px rgba(0,0,0,0.3); border: none; }";
    html += "</style>";
    html += "<script>";
    html += "function updateStatus() {";
    html += "  fetch('/status').then(r => r.json()).then(d => {";
    html += "    document.getElementById('led-status').innerText = d.led_state ? 'LED ON' : 'LED OFF';";
    html += "    document.getElementById('led-status').style.background = d.led_state ? 'linear-gradient(90deg, #56CCF2 0%, #2F80ED 100%)' : 'linear-gradient(90deg, #bdc3c7 0%, #7f8c8d 100%)';";
    html += "    document.getElementById('voice-status').innerText = d.voice_enabled ? '语音识别：已开启' : '语音识别：已关闭';";
    html += "    document.getElementById('voice-status').style.background = d.voice_enabled ? 'linear-gradient(90deg, #11998e 0%, #38ef7d 100%)' : 'linear-gradient(90deg, #bdc3c7 0%, #7f8c8d 100%)';";
    html += "    document.getElementById('stm-vo').innerText = d.stm_vo;";
    html += "    document.getElementById('stm-io').innerText = d.stm_io;";
    html += "    document.getElementById('stm-bat').innerText = d.stm_bat + ' V (' + d.stm_bat_pct + '%)';";
    html += "    document.getElementById('stm-temp').innerText = d.stm_temp;";
    html += "    document.getElementById('stm-ac').innerText = d.stm_ac;";
    html += "    document.getElementById('ip').innerText = d.ip;";
    html += "    document.getElementById('uptime').innerText = Math.floor(d.uptime / 60) + 'm ' + (d.uptime % 60) + 's';";
    html += "  });";
    html += "}";
    html += "function ledControl(action) { fetch('/led?action=' + action).then(() => updateStatus()); }";
    html += "function setColor(r,g,b) { fetch('/led?color=' + r + ',' + g + ',' + b).then(() => updateStatus()); }";
    html += "function setEffect(e) { fetch('/led?effect=' + e).then(() => updateStatus()); }";
    html += "function toggleVoice() { ";
    html += "  let enabled = document.getElementById('voice-status').innerText.includes('已开启');";
    html += "  fetch('/voice?enable=' + (enabled ? '0' : '1')).then(() => updateStatus());";
    html += "}";
    html += "function hueToRgb(h) {";
    html += "  h = h / 360;";
    html += "  let r, g, b;";
    html += "  let i = Math.floor(h * 6);";
    html += "  let f = h * 6 - i;";
    html += "  let q = 1 - f;";
    html += "  switch(i % 6) {";
    html += "    case 0: r = 1; g = f; b = 0; break;";
    html += "    case 1: r = q; g = 1; b = 0; break;";
    html += "    case 2: r = 0; g = 1; b = f; break;";
    html += "    case 3: r = 0; g = q; b = 1; break;";
    html += "    case 4: r = f; g = 0; b = 1; break;";
    html += "    case 5: r = 1; g = 0; b = q; break;";
    html += "  }";
    html += "  return [Math.round(r*255), Math.round(g*255), Math.round(b*255)];";
    html += "}";
    html += "function updateColorSlider() {";
    html += "  let hue = document.getElementById('hue-slider').value;";
    html += "  let rgb = hueToRgb(hue);";
    html += "  document.getElementById('color-preview').style.background = 'rgb(' + rgb[0] + ',' + rgb[1] + ',' + rgb[2] + ')';";
    html += "  setColor(rgb[0], rgb[1], rgb[2]);";
    html += "}";
    html += "setInterval(updateStatus, 500);";
    html += "window.onload = updateStatus;";
    html += "</script>";
    html += "</head><body>";
    html += "<div class='container'>";
    html += "<h1>🎤 ESP32 Voice Control System</h1>";
    html += "<div class='status-bar'>IP: <span id='ip'>" + WiFi.localIP().toString() + "</span> | Uptime: <span id='uptime'>0s</span></div>";
    
    html += "<h2>🎤 Voice Recognition</h2>";
    html += "<div id='voice-status' class='led-status' style='cursor: pointer;' onclick='toggleVoice()'>语音识别：已关闭</div>";
    html += "<p style='text-align: center; color: #666; font-size: 14px; margin: 5px 0;'>点击上方开关来启用/禁用语音识别</p>";
    
    html += "<h2>💡 LED Control</h2>";
    html += "<div id='led-status' class='led-status'>LED OFF</div>";
    html += "<div class='btn-group'>";
    html += "<button onclick='ledControl(\"on\")'>🔆 Turn ON</button>";
    html += "<button class='btn-off' onclick='ledControl(\"off\")'>🌙 Turn OFF</button>";
    html += "</div>";
    html += "<h2>🎨 LED Colors</h2>";
    html += "<div class='color-slider-container'>";
    html += "<div id='color-preview' class='color-preview' style='background: rgb(255,0,0);'></div>";
    html += "<input type='range' id='hue-slider' min='0' max='360' value='0' oninput='updateColorSlider()' style='margin-top: 10px;'>";
    html += "<p style='text-align: center; margin: 5px 0; color: #666; font-size: 14px;'>🌈 Drag to select color</p>";
    html += "</div>";
    html += "<div class='btn-group'>";
    html += "<button class='btn-red' onclick='setColor(255,0,0)'>🔴 Red</button>";
    html += "<button class='btn-green' onclick='setColor(0,255,0)'>🟢 Green</button>";
    html += "<button class='btn-blue' onclick='setColor(0,0,255)'>🔵 Blue</button>";
    html += "<button class='btn-yellow' onclick='setColor(255,255,0)'>🟡 Yellow</button>";
    html += "<button class='btn-purple' onclick='setColor(128,0,255)'>🟣 Purple</button>";
    html += "<button class='btn-cyan' onclick='setColor(0,255,255)'>🔷 Cyan</button>";
    html += "</div>";
    html += "<h2>✨ Light Effects</h2>";
    html += "<div class='btn-group'>";
    html += "<button style='background:#16a085;' onclick='setEffect(2)'>💫 Breath</button>";
    html += "<button style='background:#e67e22;' onclick='setEffect(3)'>🌈 Rainbow</button>";
    html += "<button style='background:#8e44ad;' onclick='setEffect(4)'>🎆 Gradient</button>";
    html += "<button style='background:#34495e;' onclick='setEffect(1)'>⚪ Solid</button>";
    html += "</div>";
    html += "</div>";
    
    html += "<h2>📊 STM32 Data (Real-time)</h2>";
    html += "<div class='info-grid'>";
    html += "<div class='info-box'><strong>Output Voltage</strong><span id='stm-vo'>--</span> V</div>";
    html += "<div class='info-box'><strong>Output Current</strong><span id='stm-io'>--</span> A</div>";
    html += "<div class='info-box'><strong>Battery</strong><span id='stm-bat'>-- V (--)</span></div>";
    html += "<div class='info-box'><strong>Temperature</strong><span id='stm-temp'>--</span> °C</div>";
    html += "<div class='info-box' style='grid-column: 1 / -1;'><strong>AC Voltage</strong><span id='stm-ac'>--</span> V</div>";
    html += "</div>";
    
    html += "<h2>💬 Send Message</h2>";
    html += "<form action='/send' method='POST'>";
    html += "<input type='text' name='message' placeholder='Enter message to display on OLED...' required>";
    html += "<button type='submit'>📤 Send to ESP32</button>";
    html += "</form>";
    
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
}

// 处理消息发送
static void handleSend() {
    if(server.hasArg("message")) {
        receivedMessage = server.arg("message");
        Serial.println("Received: " + receivedMessage);
        
        // 在OLED上显示接收到的消息
        OLED_NewFrame();
        OLED_PrintASCIIString(0, 5, "New Message:", &afont8x6, OLED_COLOR_NORMAL);
        OLED_DrawLine(0, 15, 127, 15, OLED_COLOR_NORMAL);
        OLED_PrintASCIIString(0, 25, receivedMessage.c_str(), &afont8x6, OLED_COLOR_NORMAL);
        OLED_DrawLine(0, 45, 127, 45, OLED_COLOR_NORMAL);
        OLED_PrintASCIIString(0, 50, "From: Phone", &afont8x6, OLED_COLOR_NORMAL);
        OLED_ShowFrame();
        
        // 设置消息显示标志（显示10秒）
        showingMessage = true;
        messageDisplayTime = millis();
        
        // 返回成功页面
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta http-equiv='refresh' content='2;url=/'>";
        html += "<style>body{font-family:Arial;text-align:center;padding:50px;background:#f0f0f0;}</style>";
        html += "</head><body>";
        html += "<h2>Message Sent!</h2>";
        html += "<p>" + receivedMessage + "</p>";
        html += "<p>Redirecting...</p>";
        html += "</body></html>";
        server.send(200, "text/html", html);
    } else {
        server.send(400, "text/plain", "Missing message");
    }
}

// 处理LED控制（WiFi专用，不触发语音）
static void handleLED() {
    if(server.hasArg("action")) {
        String action = server.arg("action");
        if(action == "on") {
            led_state = 1;
            current_effect = 1; // EFFECT_SOLID
            digitalWrite(LED_PIN, HIGH);
            for(int i=0; i<strip.numPixels(); i++) {
                strip.setPixelColor(i, strip.Color(led_r, led_g, led_b));
            }
            strip.show();
            Serial.println("[Web] LED ON");
        } else if(action == "off") {
            led_state = 0;
            current_effect = 0; // EFFECT_NONE
            digitalWrite(LED_PIN, LOW);
            strip.clear();
            strip.show();
            Serial.println("[Web] LED OFF");
        }
        server.send(200, "text/plain", "OK");
    } else if(server.hasArg("color")) {
        String color = server.arg("color");
        int idx1 = color.indexOf(',');
        int idx2 = color.lastIndexOf(',');
        if(idx1 > 0 && idx2 > idx1) {
            led_r = color.substring(0, idx1).toInt();
            led_g = color.substring(idx1+1, idx2).toInt();
            led_b = color.substring(idx2+1).toInt();
            current_effect = 1; // EFFECT_SOLID
            
            if(led_state) {
                for(int i=0; i<strip.numPixels(); i++) {
                    strip.setPixelColor(i, strip.Color(led_r, led_g, led_b));
                }
                strip.show();
            }
            Serial.printf("[Web] Color changed to R:%d G:%d B:%d\n", led_r, led_g, led_b);
            server.send(200, "text/plain", "OK");
        } else {
            server.send(400, "text/plain", "Invalid color format");
        }
    } else if(server.hasArg("effect")) {
        int effect = server.arg("effect").toInt();
        if(effect >= 0 && effect <= 4) {
            current_effect = effect;
            effect_millis = millis();
            if(led_state == 0) {
                led_state = 1;
                digitalWrite(LED_PIN, HIGH);
            }
            Serial.printf("[Web] Effect changed to %d\n", effect);
            server.send(200, "text/plain", "OK");
        } else {
            server.send(400, "text/plain", "Invalid effect");
        }
    } else {
        server.send(400, "text/plain", "Missing parameter");
    }
}

// 处理语音识别开关
static void handleVoice() {
    if(server.hasArg("enable")) {
        String enable = server.arg("enable");
        if(enable == "1" || enable == "true") {
            voice_recognition_enabled = true;
            playVoiceResponse("/ASR.mp3");  // 播放语音识别启动音频
            Serial.println("[Web] Voice recognition enabled");
            server.send(200, "text/plain", "OK");
        } else if(enable == "0" || enable == "false") {
            voice_recognition_enabled = false;
            Serial.println("[Web] Voice recognition disabled");
            server.send(200, "text/plain", "OK");
        } else {
            server.send(400, "text/plain", "Invalid value");
        }
    } else {
        server.send(400, "text/plain", "Missing parameter");
    }
}

// 处理状态查询
static void handleStatus() {
    STM32Data stmData = {0};
    STM32_GetData(&stmData);
    
    int batteryPct = calculateBatteryPercentage(stmData.battery_voltage);
    
    String json = "{";
    json += "\"led_state\":" + String(led_state) + ",";
    json += "\"voice_enabled\":" + String(voice_recognition_enabled ? "true" : "false") + ",";
    json += "\"counter\":" + String(counter) + ",";
    json += "\"mic_level\":" + String(micLevel) + ",";
    json += "\"message\":\"" + receivedMessage + "\",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"uptime\":" + String(millis() / 1000) + ",";
    json += "\"stm_vo\":" + String(stmData.output_voltage, 2) + ",";
    json += "\"stm_io\":" + String(stmData.output_current, 2) + ",";
    json += "\"stm_bat\":" + String(stmData.battery_voltage, 2) + ",";
    json += "\"stm_bat_pct\":" + String(batteryPct) + ",";
    json += "\"stm_temp\":" + String(stmData.temperature, 1) + ",";
    json += "\"stm_ac\":" + String(stmData.ac_voltage, 1);
    json += "}";
    
    server.send(200, "application/json", json);
}

// 初始化WiFi连接
bool WiFi_Init(const char* ssid, const char* password) {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
    
    // 在OLED上显示WiFi连接中
    OLED_NewFrame();
    OLED_PrintASCIIString(10, 10, "ESP32 OLED", &afont8x6, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(10, 25, "WiFi Init...", &afont8x6, OLED_COLOR_NORMAL);
    OLED_ShowFrame();
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while(WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if(WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        
        // 在OLED上显示WiFi信息
        OLED_NewFrame();
        OLED_PrintASCIIString(5, 5, "WiFi Connected!", &afont8x6, OLED_COLOR_NORMAL);
        OLED_DrawLine(0, 15, 127, 15, OLED_COLOR_NORMAL);
        OLED_PrintASCIIString(5, 20, "IP:", &afont8x6, OLED_COLOR_NORMAL);
        OLED_PrintASCIIString(5, 32, WiFi.localIP().toString().c_str(), &afont8x6, OLED_COLOR_NORMAL);
        char rssi[20];
        snprintf(rssi, sizeof(rssi), "RSSI:%ddBm", WiFi.RSSI());
        OLED_PrintASCIIString(5, 50, rssi, &afont8x6, OLED_COLOR_NORMAL);
        OLED_ShowFrame();
        
        delay(2000);
        return true;
    } else {
        Serial.println("\nWiFi failed! Continuing without WiFi...");
        OLED_NewFrame();
        OLED_PrintASCIIString(10, 15, "WiFi Failed!", &afont8x6, OLED_COLOR_NORMAL);
        OLED_PrintASCIIString(10, 30, "Will retry...", &afont8x6, OLED_COLOR_NORMAL);
        OLED_ShowFrame();
        delay(2000);
        return false;  // 返回false但不阻塞
    }
}

// 初始化Web服务器
void WebServer_Init() {
    server.on("/", handleRoot);
    server.on("/send", HTTP_POST, handleSend);
    server.on("/status", handleStatus);
    server.on("/led", handleLED);
    server.on("/voice", handleVoice);  // 添加语音识别开关路由
    server.begin();
    Serial.println("Web server started!");
}

// 处理Web服务器请求
void WebServer_Handle() {
    server.handleClient();
}

// 设置接收到的消息
void SetReceivedMessage(const String& msg) {
    receivedMessage = msg;
}

// 获取接收到的消息
String GetReceivedMessage() {
    return receivedMessage;
}

// 检查是否正在显示消息
bool IsShowingMessage() {
    return showingMessage;
}

// 更新消息显示状态
void UpdateMessageDisplay() {
    if(showingMessage && (millis() - messageDisplayTime > 10000)) {
        showingMessage = false;
    }
}

// 递增计数器
void IncrementCounter() {
    counter++;
}

// 获取计数器值
int GetCounter() {
    return counter;
}

// 设置麦克风音量
void SetMicLevel(int level) {
    micLevel = level;
}

// 获取麦克风音量
int GetMicLevel() {
    return micLevel;
}

// WiFi控制LED状态（不触发语音）
void SetLEDStateFromWeb(int state) {
    led_state = state;
}

// WiFi设置LED颜色（不触发语音）
void SetLEDColorFromWeb(uint8_t r, uint8_t g, uint8_t b) {
    led_r = r;
    led_g = g;
    led_b = b;
}

// 获取LED状态
int GetLEDState() {
    return led_state;
}

// 设置语音识别开关
void SetVoiceRecognitionEnabled(bool enabled) {
    voice_recognition_enabled = enabled;
}

// 获取语音识别开关状态
bool GetVoiceRecognitionEnabled() {
    return voice_recognition_enabled;
}
