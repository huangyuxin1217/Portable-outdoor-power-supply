#include <Arduino.h>
#include "oled.h"
#include "wifi_server.h"
#include "voice_recognition.h"
#include "stm32_comm.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <SPIFFS.h>
#include <Adafruit_NeoPixel.h>
#include "AudioFileSourceSPIFFS.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

#define LED_PIN 2
#define NEOPIXEL_PIN 27
#define NEOPIXEL_COUNT 60

Adafruit_NeoPixel strip(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// WiFi配置
const char* ssid = "zmy";
const char* password = "zmy060521";

// LED状态
int led_state = 0;

// 语音识别开关（默认关闭）
bool voice_recognition_enabled = false;

// LED灯光效果
enum LightEffect {
    EFFECT_NONE = 0,
    EFFECT_SOLID = 1,
    EFFECT_BREATH = 2,
    EFFECT_RAINBOW = 3,
    EFFECT_GRADIENT = 4
};
int current_effect = EFFECT_NONE;
unsigned long effect_millis = 0;

// FreeRTOS 句柄
QueueHandle_t voiceQueue;
TaskHandle_t voiceTaskHandle;

// 音频对象
AudioGeneratorMP3 *mp3;
AudioFileSourceSPIFFS *file;
AudioOutputI2S *out;
bool isPlaying = false;

// I2S 引脚定义 (MAX98357A)
#define I2S_LRC     33
#define I2S_BCLK    32
#define I2S_DOUT    23

// 语音识别任务函数
void voiceTask(void *parameter) {
    while (1) {
        if (voice_recognition_enabled) {
            VoiceCommand cmd = voice_loop();
            if (cmd != CMD_NONE) {
                xQueueSend(voiceQueue, &cmd, 0);
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// WiFi重连任务
void wifiReconnectTask(void *parameter) {
    while(1) {
        if(WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi disconnected, reconnecting...");
            WiFi.disconnect();
            delay(1000);
            WiFi.begin(ssid, password);
            
            int attempts = 0;
            while(WiFi.status() != WL_CONNECTED && attempts < 20) {
                delay(500);
                attempts++;
            }
            
            if(WiFi.status() == WL_CONNECTED) {
                Serial.println("WiFi reconnected!");
                Serial.println(WiFi.localIP());
            }
        }
        vTaskDelay(30000 / portTICK_PERIOD_MS); // 每30秒检查一次
    }
}

void playVoiceResponse(const char *filename) {
    if (mp3->isRunning()) mp3->stop();
    if (file) delete file;
    vTaskSuspend(voiceTaskHandle);
    file = new AudioFileSourceSPIFFS(filename);
    mp3->begin(file, out);
    isPlaying = true;
    Serial.printf("Playing: %s\n", filename);
}

// 计算电池电量百分比 (24V锐电池系统)
int calculateBatteryPercentage(float voltage) {
    // 24V电池系统的电压范围（6S 锂电池）
    const float VOLTAGE_MAX = 25.2;  // 满电 (4.2V x 6)
    const float VOLTAGE_MIN = 18.0;  // 空电 (3.0V x 6)
    
    if (voltage >= VOLTAGE_MAX) return 100;
    if (voltage <= VOLTAGE_MIN) return 0;
    
    // 线性插值计算百分比
    int percentage = (int)((voltage - VOLTAGE_MIN) / (VOLTAGE_MAX - VOLTAGE_MIN) * 100);
    return constrain(percentage, 0, 100);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n=================================");
    Serial.println("ESP32 Control Math System START");
    Serial.println("=================================\n");
    
    pinMode(LED_PIN, OUTPUT);
    
    // WS2812B 初始化 - 直连版本（已移除电阻）
    strip.begin();
    strip.setBrightness(30);
    strip.clear();
    strip.show();
    Serial.println("WS2812B initialized (direct connection)");

    // OLED初始化
    OLED_Init();
    
    // 语音识别
    voice_setup();
    
    // SPIFFS和音频
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
    }
    out = new AudioOutputI2S(1, AudioOutputI2S::EXTERNAL_I2S);
    out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    out->SetGain(3.5);  // 提高音频输出音量
    mp3 = new AudioGeneratorMP3();
    
    // STM32通信
    STM32_Init();
    
    // FreeRTOS队列和任务
    voiceQueue = xQueueCreate(5, sizeof(VoiceCommand));
    xTaskCreatePinnedToCore(voiceTask, "VoiceTask", 8192, NULL, 1, &voiceTaskHandle, 0);
    
    // WiFi - 失败不阻塞
    WiFi_Init(ssid, password);
    if(WiFi.status() == WL_CONNECTED) {
        WebServer_Init();
        playVoiceResponse("/web_init.mp3");  // 播放Web初始化成功提示音
        Serial.println("=================================\n");
    } else {
        Serial.println("WiFi init failed, will retry in background");
    }
    
    // 启动WiFi重连任务
    xTaskCreatePinnedToCore(wifiReconnectTask, "WiFiReconnect", 4096, NULL, 1, NULL, 1);
}

unsigned long lastOledUpdate = 0;
const unsigned long OLED_INTERVAL = 500;

void loop() {
    WebServer_Handle();

    VoiceCommand cmd;
    if (xQueueReceive(voiceQueue, &cmd, 0) == pdTRUE) {
        if (cmd == CMD_HELLO) {
            playVoiceResponse("/yes.mp3"); // 播放yes.mp3语音提示
            Serial.println("Voice Command: HELLO (Activated)");
            for(int i=0; i<10; i++) {
                strip.setPixelColor(i, strip.Color(0, 50, 255));
            }
            strip.show();
            delay(200);
            if (led_state == 0) {
                strip.clear();
                strip.show();
            }
        } else if (cmd == CMD_KAIDENG) {
            Serial.println("Voice Command: ON");
            if (led_state == 0) {
                // 只有在灯关闭时才执行开灯并播放语音
                led_state = 1;
                pinMode(LED_PIN, OUTPUT);
                digitalWrite(LED_PIN, HIGH);
                // 默认开灯为黄色
                for(int i=0; i<strip.numPixels(); i++) {
                    strip.setPixelColor(i, strip.Color(255, 255, 0));
                }
                strip.show();
                playVoiceResponse("/on.mp3");
            } else {
                Serial.println("LED already ON, skipping voice response");
            }
        } else if (cmd == CMD_GUANDENG) {
            Serial.println("Voice Command: OFF");
            if (led_state == 1) {
                // 只有在灯开启时才执行关灯并播放语音
                led_state = 0;
                digitalWrite(LED_PIN, LOW);
                strip.clear();
                strip.show();
                playVoiceResponse("/off.mp3");
            } else {
                Serial.println("LED already OFF, skipping voice response");
            }
        }
    }

    // 音频播放处理
    if (mp3->isRunning()) {
        if (!mp3->loop()) {
            mp3->stop();
            isPlaying = false;
            vTaskResume(voiceTaskHandle);
            Serial.println("Playback finished");
            
            // 恢复LED状态
            pinMode(LED_PIN, OUTPUT);
            digitalWrite(LED_PIN, led_state ? HIGH : LOW);
            if (led_state) {
                for(int i=0; i<strip.numPixels(); i++) {
                    strip.setPixelColor(i, strip.Color(255, 255, 0));
                }
                strip.show();
            }
        }
    }
    
    // LED灯光效果处理
    if (led_state && current_effect != EFFECT_NONE && current_effect != EFFECT_SOLID) {
        unsigned long now = millis();
        
        if (current_effect == EFFECT_BREATH) {
            // 呼吸灯效果
            float breath = (exp(sin((now - effect_millis) / 2000.0 * PI)) - 0.36787944) * 108.0;
            int brightness = (int)breath;
            for(int i=0; i<strip.numPixels(); i++) {
                strip.setPixelColor(i, strip.Color(brightness, brightness, brightness));
            }
            strip.show();
        } else if (current_effect == EFFECT_RAINBOW) {
            // 彩虹灯效果
            uint16_t hue = (now - effect_millis) / 10 % 65536;
            for(int i=0; i<strip.numPixels(); i++) {
                strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue + (i * 65536L / strip.numPixels()))));
            }
            strip.show();
        } else if (current_effect == EFFECT_GRADIENT) {
            // 渐变灯效果
            uint16_t hue = (now - effect_millis) / 20 % 65536;
            uint32_t color = strip.gamma32(strip.ColorHSV(hue));
            for(int i=0; i<strip.numPixels(); i++) {
                strip.setPixelColor(i, color);
            }
            strip.show();
        }
    }
    
    // OLED更新
    if (!isPlaying && (millis() - lastOledUpdate > OLED_INTERVAL)) {
        lastOledUpdate = millis();
        UpdateMessageDisplay();
        
        if(!IsShowingMessage()) {
            OLED_NewFrame();
            OLED_PrintASCIIString(0, 0, led_state ? "LED: ON " : "LED: OFF", &afont8x6, OLED_COLOR_NORMAL);

            STM32Data stmData;
            if (STM32_GetData(&stmData)) {
                char buf[20];
                snprintf(buf, sizeof(buf), "Vo:%.1fV", stmData.output_voltage);
                OLED_PrintASCIIString(0, 12, buf, &afont8x6, OLED_COLOR_NORMAL);
                snprintf(buf, sizeof(buf), "Io:%.2fA", stmData.output_current);
                OLED_PrintASCIIString(64, 12, buf, &afont8x6, OLED_COLOR_NORMAL);
                
                // 电池电压和电量百分比
                int batteryPct = calculateBatteryPercentage(stmData.battery_voltage);
                snprintf(buf, sizeof(buf), "Bat:%.1fV %d%%", stmData.battery_voltage, batteryPct);
                OLED_PrintASCIIString(0, 24, buf, &afont8x6, OLED_COLOR_NORMAL);
                
                snprintf(buf, sizeof(buf), "T:%.1fC", stmData.temperature);
                OLED_PrintASCIIString(0, 36, buf, &afont8x6, OLED_COLOR_NORMAL);
                snprintf(buf, sizeof(buf), "AC:%.0fV", stmData.ac_voltage);
                OLED_PrintASCIIString(64, 36, buf, &afont8x6, OLED_COLOR_NORMAL);
            } else {
                OLED_PrintASCIIString(0, 20, "Waiting for STM32...", &afont8x6, OLED_COLOR_NORMAL);
            }
            
            char ip[20];
            snprintf(ip, sizeof(ip), "IP:%s", WiFi.localIP().toString().c_str());
            OLED_PrintASCIIString(0, 54, ip, &afont8x6, OLED_COLOR_NORMAL);
            OLED_ShowFrame();
        }
    }
}
