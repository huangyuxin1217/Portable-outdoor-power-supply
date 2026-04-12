#ifndef OLED_H
#define OLED_H

#include <Wire.h>
#include <stdint.h>

// OLED dimensions
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_ADDRESS 0x3C

// Color modes
typedef enum {
  OLED_COLOR_NORMAL = 0,   // Normal mode: black background, white text
  OLED_COLOR_REVERSED = 1  // Reversed mode: white background, black text
} OLED_ColorMode;

// ASCII Font structure
typedef struct {
  uint8_t h;
  uint8_t w;
  const uint8_t *chars;
} ASCIIFont;

// Basic functions
void OLED_Init();
void OLED_DisPlay_On();
void OLED_DisPlay_Off();

// Frame buffer operations
void OLED_NewFrame();
void OLED_ShowFrame();
void OLED_SetPixel(uint8_t x, uint8_t y, OLED_ColorMode color);

// Drawing functions
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, OLED_ColorMode color);
void OLED_DrawRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, OLED_ColorMode color);
void OLED_DrawFilledRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, OLED_ColorMode color);
void OLED_DrawCircle(uint8_t x, uint8_t y, uint8_t r, OLED_ColorMode color);
void OLED_DrawFilledCircle(uint8_t x, uint8_t y, uint8_t r, OLED_ColorMode color);

// Text functions
void OLED_PrintASCIIChar(uint8_t x, uint8_t y, char ch, const ASCIIFont *font, OLED_ColorMode color);
void OLED_PrintASCIIString(uint8_t x, uint8_t y, const char *str, const ASCIIFont *font, OLED_ColorMode color);

// Font definitions
extern const ASCIIFont afont8x6;
extern const ASCIIFont afont12x6;
extern const ASCIIFont afont16x8;

#endif
