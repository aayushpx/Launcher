#include "event_log.h"

#include <stdio.h>
#include <string.h>
#include "esp_timer.h"

typedef struct {
    int64_t timestamp_us;
    char message[EVENT_LOG_MESSAGE_SIZE];
} mission_event_t;

static mission_event_t events[EVENT_LOG_SIZE];
static int next_event = 0;
static int event_count = 0;

void event_log_init(void)
{
    memset(events, 0, sizeof(events));
    next_event = 0;
    event_count = 0;
}

void event_log_add(const char *message)
{
    if (message == NULL) {
        return;
    }

    mission_event_t *event = &events[next_event];

    event->timestamp_us = esp_timer_get_time();
    snprintf(event->message, sizeof(event->message), "%s", message);

    next_event = (next_event + 1) % EVENT_LOG_SIZE;

    if (event_count < EVENT_LOG_SIZE) {
        event_count++;
    }
}

int event_log_count(void)
{
    return event_count;
}

int event_log_get_recent(int index,
                         char *message,
                         size_t message_size,
                         int64_t *timestamp_us)
{
    if (index < 0 || index >= event_count ||
        message == NULL || message_size == 0) {
        return 0;
    }

    int slot = (next_event - 1 - index + EVENT_LOG_SIZE)
               % EVENT_LOG_SIZE;

    snprintf(message, message_size, "%s", events[slot].message);

    if (timestamp_us != NULL) {
        *timestamp_us = events[slot].timestamp_us;
    }

    return 1;
}
