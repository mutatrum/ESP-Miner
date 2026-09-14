#ifndef DISPLAY_ST7789_H_
#define DISPLAY_ST7789_H_

#include <stdbool.h>
#include "esp_err.h"
#include "display.h"

typedef struct GlobalState GlobalState;

#define LCD_I80_PIXEL_CLOCK_HZ  4000000
#define LCD_I80_BUF_LINES       40
#define LCD_I80_H_RES           320
#define LCD_I80_V_RES           170
#define LCD_I80_GAP_X           0
#define LCD_I80_GAP_Y           35

#define LCD_BK_LIGHT_ON_LEVEL   1
#define LCD_BK_LIGHT_OFF_LEVEL  0
#define LCD_PWR_ON_LEVEL        1
#define LCD_PWR_OFF_LEVEL       0

extern const DisplayDriver display_st7789_driver;

bool display_st7789_probe(GlobalState * GLOBAL_STATE);

#endif /* DISPLAY_ST7789_H_ */
