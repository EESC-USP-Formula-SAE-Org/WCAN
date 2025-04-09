#include <stdio.h>

#include "time_t.h"

// Sets the RTC time to the specified 'seconds' value (epoch time)
void set_rtc_time(time_t seconds) {
    struct timeval tv = { .tv_sec = seconds, .tv_usec = 0 };
    // Configure the system time with the new settings
    if (settimeofday(&tv, NULL) != 0) {
        printf("Failed to set RTC time\n");
    }
}

// Returns the current time in seconds since the UNIX epoch
time_t get_rtc_seconds() {
    return time(NULL);
}
