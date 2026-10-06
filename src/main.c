#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>

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

void app_main()
{
  input_output_init();
  graphics_init();
  cls(0);

  launch_init();

  while (1) {
    launch_program();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

