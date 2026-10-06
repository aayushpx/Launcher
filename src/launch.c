#include "launch.h"
static launch_state_t current_state = STATE_SAFE;

void launch_init(void) {
  current_state = STATE_SAFE;
}

void launch_process_event(launch_event_t event) {
  switch (current_state) {
    case STATE_SAFE:
      if (event == EVENT_ARM) {
        current_state = STATE_ARMED;
      }
      break;

    case STATE_ARMED:
      if (event == EVENT_START) {
        current_state = STATE_COUNTDOWN;
      }
      break;

    case STATE_COUNTDOWN:
      break;

    case STATE_LAUNCH:
      break;

    case STATE_FLIGHT:
      break;

    case STATE_LANDED:
      break;
  }

  if (event == EVENT_ABORT) {
    current_state = STATE_SAFE;
  }
}

launch_state_t launch_get_state(void) {
  return current_state;
}
