#ifndef LAUNCH_H
#define LAUNCH_H

typedef enum {
  STATE_SAFE,
  STATE_ARMED,
  STATE_COUNTDOWN,
  STATE_LAUNCH,
  STATE_FLIGHT,
  STATE_LANDED
} launch_state_t;

typedef enum {
  EVENT_ARM,
  EVENT_START,
  EVENT_ABORT
} launch_event_t;

void launch_init(void);
void launch_process_event(launch_event_t event);
launch_state_t launch_get_state(void);

#endif
