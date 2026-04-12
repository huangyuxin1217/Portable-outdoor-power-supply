#ifndef WIFI_SERVER_H
#define WIFI_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// WiFi和服务器控制函数
bool WiFi_Init(const char* ssid, const char* password);
void WebServer_Init();
void WebServer_Handle();

// 消息处理函数
void SetReceivedMessage(const String& msg);
String GetReceivedMessage();
bool IsShowingMessage();
void UpdateMessageDisplay();

// 计数器管理
void IncrementCounter();
int GetCounter();

// 麦克风数据
void SetMicLevel(int level);
int GetMicLevel();

// LED控制（WiFi专用，不触发语音）
void SetLEDStateFromWeb(int state);
void SetLEDColorFromWeb(uint8_t r, uint8_t g, uint8_t b);
int GetLEDState();

// 语音识别开关控制
void SetVoiceRecognitionEnabled(bool enabled);
bool GetVoiceRecognitionEnabled();

#endif
