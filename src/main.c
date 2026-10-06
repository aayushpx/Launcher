#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include "leds.h"
#include "esp_err.h"

#include "fonts.h"
#include "graphics.h"
#include "launch.h"

#ifdef TTGO_S3
#define RIGHT_BUTTON 14
#else
#define RIGHT_BUTTON 35
#endif

#define LEFT_BUTTON 0

void input_output_init();

static void launch_program()
{
  set_orientation(LANDSCAPE);

  while (1) {

    cls(rgbToColour(0, 0, 10));

    /*
     * Input
     */
    if (!gpio_get_level(LEFT_BUTTON)) {
      launch_process_event(EVENT_ARM);
    }

    if (!gpio_get_level(RIGHT_BUTTON)) {
      launch_process_event(EVENT_START);
    }

    /*
     * Display
     */
    setFont(FONT_DEJAVU18);
    setFontColour(255, 255, 255);
    print_xy("LAUNCH", CENTER, 45);

    setFont(FONT_UBUNTU16);
    setFontColour(100, 180, 255);
    print_xy("FLIGHT COMPUTER", CENTER, 85);

    setFont(FONT_UBUNTU16);
    setFontColour(255, 255, 255);

    switch (launch_get_state()) {

      case STATE_SAFE:
        print_xy("SAFE", CENTER, 130);
        break;

      case STATE_ARMED:
        print_xy("ARMED", CENTER, 130);
        break;

      case STATE_COUNTDOWN:
        print_xy("COUNTDOWN", CENTER, 130);
        break;

      case STATE_LAUNCH:
        print_xy("LAUNCH", CENTER, 130);
        break;

      case STATE_FLIGHT:
        print_xy("FLIGHT", CENTER, 130);
        break;

      case STATE_LANDED:
        print_xy("LANDED", CENTER, 130);
        break;
    }

    flip_frame();

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

<<<<<<< HEAD
void app_main()
{
=======
typedef enum {
  MISSION_SAFE,  // 0
  MISSION_ARMED,  // 1
  MISSION_COUNTDOWN,  // 2
  MISSION_FLIGHT,  // 3
  MISSION_LANDED,   // 4
  MISSION_ABORTED  // 5
} mission_state_t;



static void draw_mission_screen(mission_state_t state, const char *timer_text) {
  set_orientation(LANDSCAPE);
  setFont(FONT_UBUNTU16);
  setFontColour(255, 255, 255);

  cls(rgbToColour(5, 10, 20));

  print_xy("MISSION CONTROL", 10, 10);
  print_xy("----------------", 10, 35);

  switch (state) {
    case MISSION_SAFE:
      setFontColour(180, 180, 180);
      print_xy("SAFE", 10, 65);
      break;

    case MISSION_ARMED:
      setFontColour(0, 255, 100);
      print_xy("ARMED", 10, 65);
      break;

    case MISSION_COUNTDOWN:
      setFontColour(255, 180, 0);
      print_xy("COUNTDOWN", 10, 65);
      break;

    case MISSION_FLIGHT:
      setFontColour(0, 200, 255);
      print_xy("FLIGHT", 10, 65);
      break;

    case MISSION_LANDED:
      setFontColour(0, 255, 100);
      print_xy("LANDED", 10, 65);
      break;

    case MISSION_ABORTED:
      setFontColour(255, 40, 40);
      print_xy("ABORTED", 10, 65);
      break;
  }

  if (timer_text[0] != '\0') {
    setFontColour(255, 255, 255);
    print_xy(timer_text, 10, 100);
  }

  flip_frame();
}


static uint32_t pulse_brightness(int64_t now_us, int period_ms)
{
    int64_t period_us = (int64_t)period_ms * 1000;
    int64_t phase = now_us % period_us;

    uint32_t triangle;

    if (phase < period_us / 2) {
        triangle = (uint32_t)(phase * 2046 / period_us);
    } else {
        triangle = (uint32_t)((period_us - phase) * 2046 / period_us);
    }

    return 80 + (uint32_t)((uint64_t)triangle * 943 / 1023);
}

static void update_mission_leds(
    mission_state_t state,
    int64_t now_us,
    int64_t countdown_start_us)
{
  uint32_t green = 0;
  uint32_t blue = 0;
  uint32_t red = 0;

  switch (state) {
    case MISSION_SAFE:
      green = pulse_brightness(now_us, 1700);
      break;

    case MISSION_ARMED:
      green = 1023;
      blue = pulse_brightness(now_us, 800);
      break;

    case MISSION_COUNTDOWN: {
                              blue = 1023;

                              int64_t elapsed = now_us - countdown_start_us;
                              int seconds_left = 10 - (int)(elapsed / 1000000);

                              int pulse_period;

                              if (seconds_left > 7) {
                                pulse_period = 1000;
                              } else if (seconds_left > 4) {
                                pulse_period = 650;
                              } else if (seconds_left > 2) {
                                pulse_period = 400;
                              } else {
                                pulse_period = 200;
                              }

                              red = pulse_brightness(now_us, pulse_period);
                              break;
                            }

    case MISSION_FLIGHT:
                            blue = 1023;
                            red = pulse_brightness(now_us, 1400);
                            break;

    case MISSION_LANDED:
                            green = 1023;
                            break;

    case MISSION_ABORTED:
                            red = 1023;
                            break;
  }

  ESP_ERROR_CHECK(led_set_brightness(10, green));
  ESP_ERROR_CHECK(led_set_brightness(11, blue));
  ESP_ERROR_CHECK(led_set_brightness(12, red));
}

void app_main(void) {
>>>>>>> 5c71996 (pwm, draft dashboard, states working)
  input_output_init();

  graphics_init();
  set_orientation(LANDSCAPE);
  cls(0);

<<<<<<< HEAD
  launch_init();

  while (1) {
    launch_program();
    vTaskDelay(pdMS_TO_TICKS(100));
=======
  ESP_ERROR_CHECK(leds_init());

  printf("Mission Control: ready\n");

  mission_state_t current_state = MISSION_SAFE;

  int prev_arm = 1;
  int prev_launch = 1;
  int prev_abort = 1;
  int64_t countdown_start_us = 0;
  int64_t flight_start_us = 0;
  const int64_t countdown_duration_us = 10LL*1000000;


  while (1) {
    int arm_now = gpio_get_level(1);
    int launch_now = gpio_get_level(2);
    int abort_now = gpio_get_level(3);

    if (prev_arm == 1 && arm_now == 0) {
      if (current_state == MISSION_SAFE) {
        current_state = MISSION_ARMED;
      } else if (current_state == MISSION_ARMED) {
        current_state = MISSION_SAFE;
      }
    }

    if (prev_launch == 1 && launch_now == 0) {
      if (current_state == MISSION_ARMED) {
        countdown_start_us = esp_timer_get_time();
        current_state = MISSION_COUNTDOWN;
      }
    }

    if (prev_abort == 1 && abort_now == 0) {
      if (current_state == MISSION_COUNTDOWN ||
          current_state == MISSION_FLIGHT) {
        current_state = MISSION_ABORTED;
      }
    }

    int64_t now_us = esp_timer_get_time();
    char timer_text[32] = "";

    if (current_state == MISSION_COUNTDOWN) {
      int64_t elapsed_us = now_us - countdown_start_us;

      if (elapsed_us >= countdown_duration_us) {
        flight_start_us = countdown_start_us + countdown_duration_us;
        current_state = MISSION_FLIGHT;
      } else {
        int seconds_left = 10 - (int)(elapsed_us / 1000000);

        snprintf(timer_text, sizeof(timer_text),
            "T-00:%02d", seconds_left);
      }
    }

    if (current_state == MISSION_FLIGHT) {
      int64_t elapsed_seconds = (now_us - flight_start_us) / 1000000;
      int minutes = (int)(elapsed_seconds / 60);
      int seconds = (int)(elapsed_seconds % 60);

      snprintf(timer_text, sizeof(timer_text),
          "T+%02d:%02d", minutes, seconds);
    }

    update_mission_leds(current_state, now_us, countdown_start_us);

    prev_arm = arm_now;
    prev_launch = launch_now;
    prev_abort = abort_now;

    draw_mission_screen(current_state, timer_text);

    printf("Current mission state: %d\n", current_state);

    vTaskDelay(pdMS_TO_TICKS(20));
>>>>>>> 5c71996 (pwm, draft dashboard, states working)
  }
}

