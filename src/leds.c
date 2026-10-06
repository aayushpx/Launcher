#include "leds.h"

#include "driver/ledc.h"

#define LED_PWM_MODE       LEDC_LOW_SPEED_MODE
#define LED_PWM_TIMER      LEDC_TIMER_0
#define LED_PWM_FREQUENCY  5000
#define LED_PWM_RESOLUTION LEDC_TIMER_10_BIT

#define LED_GREEN_GPIO 10
#define LED_BLUE_GPIO  11
#define LED_RED_GPIO   12

static esp_err_t configure_led(uint8_t gpio, ledc_channel_t channel)
{
  ledc_channel_config_t config = {
    .gpio_num = gpio,
    .speed_mode = LED_PWM_MODE,
    .channel = channel,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = LED_PWM_TIMER,
    .duty = 0,
    .hpoint = 0,
  };

  return ledc_channel_config(&config);
}

esp_err_t leds_init(void)
{
  ledc_timer_config_t timer = {
    .speed_mode = LED_PWM_MODE,
    .duty_resolution = LED_PWM_RESOLUTION,
    .timer_num = LED_PWM_TIMER,
    .freq_hz = LED_PWM_FREQUENCY,
    .clk_cfg = LEDC_AUTO_CLK,
  };

  esp_err_t err = ledc_timer_config(&timer);
  if (err != ESP_OK) {
    return err;
  }

  err = configure_led(LED_GREEN_GPIO, LEDC_CHANNEL_0);
  if (err != ESP_OK) return err;

  err = configure_led(LED_BLUE_GPIO, LEDC_CHANNEL_1);
  if (err != ESP_OK) return err;

  return configure_led(LED_RED_GPIO, LEDC_CHANNEL_2);
}

esp_err_t led_set_brightness(uint8_t gpio, uint32_t brightness)
{
  if (brightness > 1023) {
    brightness = 1023;
  }

  ledc_channel_t channel;

  switch (gpio) {
    case LED_GREEN_GPIO: channel = LEDC_CHANNEL_0; break;
    case LED_BLUE_GPIO:  channel = LEDC_CHANNEL_1; break;
    case LED_RED_GPIO:   channel = LEDC_CHANNEL_2; break;
    default: return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = ledc_set_duty(LED_PWM_MODE, channel, brightness);
  if (err != ESP_OK) return err;

  return ledc_update_duty(LED_PWM_MODE, channel);
}
