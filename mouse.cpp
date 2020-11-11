#include "mouse.h"
#include "animal_info_logic.h"
#include <unistd.h>
#include "aesm_long_lived_thread.h"


#define MICROSECONDS_PER_SECOND 1000000LL


static ThreadStatus mouse_thread;

ae_error_t MouseIOCache::entry() {
    AnimalInfoLogic::check_eat_thread_func();
    return AE_SUCCESS;
}

ThreadStatus& MouseIOCache::get_thread() {
    return mouse_thread;
}
ae_error_t start_mouse_thread(unsigned long timeout) {
    INIT_THREAD(MouseIOCache, timeout, ())
//    COPY_OUTPUT();
    FINI_THREAD()
}
