#ifndef DISPLAY_OLED_H_
#define DISPLAY_OLED_H_

#include <stdbool.h>
#include "esp_err.h"
#include "display_driver.h"

typedef struct GlobalState GlobalState;

#define DISPLAY_I2C_ADDRESS                    0x3C

extern const DisplayDriver display_oled_driver;

bool display_oled_probe(void);

#endif /* DISPLAY_OLED_H_ */
