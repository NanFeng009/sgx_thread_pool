#ifndef _SE_TIME_H
#define _SE_TIME_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    uint64_t se_get_tick_count_freq(void);
    uint64_t se_get_tick_count(void);

#ifdef __cplusplus
}
#endif
#endif
