#ifndef TIME_T_H
#define TIME_T_H
#include <time.h>
#include <sys/time.h>

// Sets the RTC time to the specified 'seconds' value (epoch time)
void set_rtc_time(time_t seconds);

// Returns the current time in seconds since the UNIX epoch
time_t get_rtc_seconds();

#endif // TIME_T_H

