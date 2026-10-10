#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include <stddef.h>
#include <stdint.h>

#define EVENT_LOG_SIZE 16
#define EVENT_LOG_MESSAGE_SIZE 32

void event_log_init(void);
void event_log_add(const char *message);
int event_log_count(void);

/* Index 0 returns the newest event. */
int event_log_get_recent(int index,
                         char *message,
                         size_t message_size,
                         int64_t *timestamp_us);

#endif
