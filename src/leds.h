#ifndef LEDS_H
#define LEDS_H

#include "esp_err.h"
#include <stdint.h>

esp_err_t leds_init(void);
esp_err_t led_set_brightness(uint8_t gpio, uint32_t brightness);

#endif
