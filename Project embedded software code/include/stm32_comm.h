#ifndef STM32_COMM_H
#define STM32_COMM_H

#include <Arduino.h>

struct STM32Data {
    float output_voltage;
    float output_current;
    float battery_voltage;
    float temperature;
    float ac_voltage;
};

// 初始化 STM32 通信
void STM32_Init();

// 获取最新数据，如果获取成功返回 true
bool STM32_GetData(STM32Data *data);

#endif
