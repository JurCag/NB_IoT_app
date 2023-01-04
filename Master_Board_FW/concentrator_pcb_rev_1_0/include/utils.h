#ifndef __UTILS_H__
#define __UTILS_H__

#define MS_TO_TICKS(ms)             (ms/portTICK_PERIOD_MS)
#define TASK_DELAY_MS(ms)           (vTaskDelay(MS_TO_TICKS(ms)))

#endif // __UTILS_H__
