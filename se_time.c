#include "se_time.h"
#include <time.h>

#define NANOSECONDS_PER_SECOND 1000000000ULL
uint64_t se_get_tick_count(void)
{
    struct timespec tm;
    if(clock_gettime(CLOCK_MONOTONIC, &tm) != 0)
        return 0;
    return ((uint64_t)tm.tv_sec * NANOSECONDS_PER_SECOND) + ((uint64_t)tm.tv_nsec);
}


uint64_t se_get_tick_count_freq(void)
{
    return NANOSECONDS_PER_SECOND;
}
