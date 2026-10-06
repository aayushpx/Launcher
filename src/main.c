#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "fonts.h"
#include "graphics.h"
#include "launch.h"

void app_main(void) {
  graphics_init();
  set_orientation(LANDSCAPE);

  while(1) {
    cls(rgbToColour(0, 0, 10));
    setFont(FONT_DEJAVU18);
    setFontColour(255, 255, 255);
    print_xy("LAUNCH",CENTER, 45);

    setFont(FONT_UBUNTU16);
    setFontColour(100, 180, 255);
    print_xy("FLIGHT COMPUTER",CENTER, 85);

    setFont(FONT_SMALL);
    setFontColour(100, 255, 100);
    print_xy("SYSTEM OK",CENTER, 130);

    setFont(FONT_UBUNTU16);
    setFontColour(255, 255, 255);

    switch (launch_get_state()) {
      case STATE_SAFE:
        print_xy("SAFE", CENTER, 180);
        break;
        
      case STATE_ARMED:
        print_xy("ARMED", CENTER, 180);
        break;

      case STATE_COUNTDOWN:
        print_xy("COUNTDOWN", CENTER, 180);
        break;

      case STATE_LAUNCH:
        print_xy("LAUNCH", CENTER, 180);
        break;

      case STATE_FLIGHT:
        print_xy("FLIGHT", CENTER, 180);
        break;

      case STATE_LANDED:
        print_xy("LANDED", CENTER, 180);
        break;
    }

    flip_frame();
    vTaskDelay(pdMS_TO_TICKS(100));

  }
}
