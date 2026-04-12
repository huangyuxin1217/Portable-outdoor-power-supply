#ifndef VOICE_RECOGNITION_H
#define VOICE_RECOGNITION_H

#include <Arduino.h>

// 定义识别到的命令枚举
enum VoiceCommand {
    CMD_NONE,      // 未识别或噪音
    CMD_HELLO,     // 激活词
    CMD_KAIDENG,   // 开灯
    CMD_GUANDENG   // 关灯
};

// 初始化语音识别模块 (I2S配置等)
void voice_setup();

// 执行一次语音识别循环
// 返回值: 识别到的命令
VoiceCommand voice_loop();

// 仅用于训练：原始数据透传
void voice_data_forwarder_loop();

#endif
