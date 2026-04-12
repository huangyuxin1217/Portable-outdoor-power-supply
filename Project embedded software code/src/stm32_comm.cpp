#include "stm32_comm.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// STM32通信引脚
#define RXD2 18  // 接STM32的TX
#define TXD2 19  // 接STM32的RX

static STM32Data latest_data;
static SemaphoreHandle_t dataMutex;
static bool data_valid = false;

void stm32Task(void *parameter) {
    uint8_t buffer[24];
    int byte_index = 0;
    
    while(1) {
        // 读取所有可用数据
        while (Serial2.available()) {
            uint8_t byte = Serial2.read();
            
            // 简单的状态机解析
            // 调试：打印收到的所有数据，排查是否收到信号
            // Serial.printf("%02X ", byte); 

            if (byte_index == 0) {
                // 寻找帧头 0xAA
                if (byte == 0xAA) {
                    buffer[byte_index++] = byte;
                } else {
                    // 如果不是AA，且我们正在等待帧头，打印一下收到的垃圾数据
                    // Serial.printf("[STM32] Wait Header AA, got %02X\n", byte);
                }
            } else {
                buffer[byte_index++] = byte;
                
                // 接收完整一帧 (24字节)
                if (byte_index == 24) {
                    Serial.println(); // 换行
                    Serial.print("[STM32] Raw Frame: ");
                    for(int k=0; k<24; k++) Serial.printf("%02X ", buffer[k]);
                    Serial.println();

                    // 1. 验证帧尾 0x55
                    // 2. 验证数据长度 20
                    if (buffer[23] == 0x55 && buffer[1] == 20) {
                        // 3. 计算校验和
                        uint8_t checksum = 0;
                        for (int i = 1; i < 22; i++) {
                            checksum += buffer[i];
                        }
                        
                        // 4. 校验通过，更新数据
                        if (checksum == buffer[22]) {
                            xSemaphoreTake(dataMutex, portMAX_DELAY);
                            memcpy(&latest_data.output_voltage, &buffer[2], 4);
                            memcpy(&latest_data.output_current, &buffer[6], 4);
                            memcpy(&latest_data.battery_voltage, &buffer[10], 4);
                            memcpy(&latest_data.temperature, &buffer[14], 4);
                            memcpy(&latest_data.ac_voltage, &buffer[18], 4);
                            data_valid = true;
                            xSemaphoreGive(dataMutex);

                            // 调试输出，确认收到有效帧
                            Serial.printf("[STM32] Frame OK! Vo=%.2f Io=%.2f\n", latest_data.output_voltage, latest_data.output_current);
                        } else {
                            // 校验失败，打印调试信息
                            Serial.printf("[STM32] Checksum ERROR: calc=0x%02X recv=0x%02X\n", checksum, buffer[22]);
                        }
                    } else {
                         Serial.printf("[STM32] Format Error: Len=%d (exp 20), Tail=%02X (exp 55)\n", buffer[1], buffer[23]);
                    }
                    // 重置索引，准备接收下一帧
                    byte_index = 0;
                }
            }

        }
        // 稍微延时，避免任务看门狗触发，同时让出CPU
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void STM32_Init() {
    // 初始化Serial2用于STM32通信
    Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
    
    // 创建互斥锁
    dataMutex = xSemaphoreCreateMutex();
    
    // 创建接收任务
    xTaskCreatePinnedToCore(
        stm32Task,
        "STM32Task",
        4096,
        NULL,
        1,
        NULL,
        1 // 运行在 Core 1
    );
}

bool STM32_GetData(STM32Data *data) {
    if (!dataMutex) {
        return false;
    }

    // 如果尚未收到有效数据，直接返回 false
    if (!data_valid) {
        return false;
    }

    // 尝试在短时间内获取互斥锁，避免频繁返回 false
    if (xSemaphoreTake(dataMutex, 5 / portTICK_PERIOD_MS) == pdTRUE) {
        *data = latest_data;
        xSemaphoreGive(dataMutex);
        return true;
    } else {
        // 退化路径：未拿到锁也返回当前快照，避免 UI 长期显示 waiting
        *data = latest_data;
        return true;
    }
}
