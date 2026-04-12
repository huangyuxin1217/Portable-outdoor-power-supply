#include "voice_recognition.h"
#include <driver/i2s.h>
#include <haofan0521-project-1_inferencing.h>

// --- 硬件配置 ---
#define I2S_SCK 26
#define I2S_WS  25
#define I2S_SD  34
#define I2S_PORT I2S_NUM_0

// --- 音频配置 ---
#define SAMPLE_RATE 16000
#define CONFIDENCE_THRESHOLD 0.85

// --- 激活词配置 ---
#define ACTIVATION_TIMEOUT 8000  // 激活后8秒内有效
static bool is_activated = false;  // 激活状态标志
static unsigned long activation_time = 0;  // 激活时间戳 

// 推理缓冲区
static float inference_buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

// 滑动窗口配置
// 每次只更新 1/3 的数据 (约 333ms)，保留 2/3 的旧数据
// 这样可以捕捉到跨越时间窗口的关键词，大大提高识别率
#define SLICE_SIZE (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE / 3)

void voice_setup() {
  Serial.println("Initializing Voice Recognition...");

  // --- I2S 初始化 ---
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 512,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  if (i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL) != ESP_OK) {
    Serial.println("I2S driver install failed");
    while (1);
  }
  if (i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) {
    Serial.println("I2S pin config failed");
    while (1);
  }
  
  // 预先清空缓冲区
  for (int i=0; i<EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; i++) {
      inference_buffer[i] = 0;
  }
  
  Serial.println("Voice Recognition Ready. Say 'KaiDeng' or 'GuanDeng'...");
}

VoiceCommand voice_loop() {
  size_t bytes_read;
  int32_t i2s_buffer[512]; 
  
  // 1. 滑动窗口处理
  // 将旧数据向前移动 (丢弃最旧的 SLICE_SIZE 数据)
  // 移动大小 = (总大小 - 新数据大小) * float字节数
  memmove(inference_buffer, 
          &inference_buffer[SLICE_SIZE], 
          (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - SLICE_SIZE) * sizeof(float));

  // 2. 录制新音频 (只填补末尾的 SLICE_SIZE)
  int samples_recorded = 0;
  // 指向缓冲区末尾空出的位置
  float *buffer_ptr = &inference_buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - SLICE_SIZE];

  while (samples_recorded < SLICE_SIZE) {
    i2s_read(I2S_PORT, (char *)i2s_buffer, sizeof(i2s_buffer), &bytes_read, portMAX_DELAY);
    
    int samples_in_chunk = bytes_read / 4; 

    for (int i = 0; i < samples_in_chunk; i += 2) { 
      if (samples_recorded < SLICE_SIZE) {
        // 关键处理：左声道 + 右移14位 (数字增益)
        // 如果声音太小识别不到，可以尝试改为 >> 13 (放大2倍)
        // 如果声音太大有杂音，可以尝试改为 >> 15 (缩小2倍)
        int32_t raw = i2s_buffer[i];
        int16_t pcm = raw >> 14; 
        buffer_ptr[samples_recorded] = (float)pcm;
        samples_recorded++;
      }
    }
  }

  // 3. 运行推理 (对整个缓冲区)
  signal_t signal;
  numpy::signal_from_buffer(inference_buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);

  ei_impulse_result_t result = { 0 };
  EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
  if (err != EI_IMPULSE_OK) {
      Serial.printf("ERR: Failed to run classifier (%d)\n", err);
      return CMD_NONE;
  }

  // 3. 解析结果
  float hello_score = 0;
  float kaideng_score = 0;
  float guandeng_score = 0;

  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    if (strcmp(result.classification[ix].label, "Hello") == 0) {
      hello_score = result.classification[ix].value;
    }
    if (strcmp(result.classification[ix].label, "KaiDeng") == 0) {
      kaideng_score = result.classification[ix].value;
    }
    if (strcmp(result.classification[ix].label, "GuanDeng") == 0) {
      guandeng_score = result.classification[ix].value;
    }
  }

  // 4. 激活词逻辑
  static unsigned long last_trigger_time = 0;
  
  // 防止同一句话重复触发（1秒冷却）
  if (millis() - last_trigger_time < 1000) {
      return CMD_NONE;
  }

  // 检查激活状态是否超时
  if (is_activated && (millis() - activation_time > ACTIVATION_TIMEOUT)) {
    is_activated = false;
    Serial.println("[激活超时] 回到待机状态");
  }

  // 优先检测 HELLO（激活词）
  if (hello_score > CONFIDENCE_THRESHOLD) {
    Serial.printf(">>> 识别成功: HELLO (%.2f) - 已激活5秒 <<<\n", hello_score);
    is_activated = true;
    activation_time = millis();
    last_trigger_time = millis();
    return CMD_HELLO;
  }

  // 只有在激活状态下才识别开灯/关灯
  if (is_activated) {
    if (kaideng_score > CONFIDENCE_THRESHOLD) {
      Serial.printf(">>> 识别成功: 开灯 (%.2f) <<<\n", kaideng_score);
      is_activated = false;  // 执行指令后退出激活状态
      last_trigger_time = millis();
      return CMD_KAIDENG;
    } 
    else if (guandeng_score > CONFIDENCE_THRESHOLD) {
      Serial.printf(">>> 识别成功: 关灯 (%.2f) <<<\n", guandeng_score);
      is_activated = false;  // 执行指令后退出激活状态
      last_trigger_time = millis();
      return CMD_GUANDENG;
    }
  }
  
  // 打印调试信息（可选）
  if (hello_score > 0.5 || kaideng_score > 0.5 || guandeng_score > 0.5) {
    Serial.printf("[调试] HELLO:%.2f KaiDeng:%.2f GuanDeng:%.2f (激活:%s)\n", 
                  hello_score, kaideng_score, guandeng_score, 
                  is_activated ? "是" : "否");
  }
  
  return CMD_NONE;
}

void voice_data_forwarder_loop() {
    int32_t i2s_buffer[512];
    size_t bytes_read;
    
    // 读取 I2S 数据
    i2s_read(I2S_NUM_0, (char *)i2s_buffer, sizeof(i2s_buffer), &bytes_read, portMAX_DELAY);
    
    int samples = bytes_read / 4;
    for (int i=0; i<samples; i+=2) { // 仅仅取单声道(左声道)
        int32_t raw = i2s_buffer[i];
        int16_t pcm = raw >> 14; 
        
        // Edge Impulse Data Forwarder 格式: 直接打印数值，换行分隔
        Serial.println(pcm);
    }
}
