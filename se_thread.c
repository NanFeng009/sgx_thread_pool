#include "se_thread.h"
#include "string.h"
#include <pthread.h>
#include <sys/syscall.h>
#include <linux/unistd.h>
#include <unistd.h>

/*
 * mutex
 */
void se_mutex_init(se_mutex_t* mutex)
{
    se_mutex_t tmp = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

    /* C doesn't allow `*mutex = PTHREAD_..._INITIALIZER`.*/
    memcpy(mutex, &tmp, sizeof(tmp));
}


int se_mutex_lock(se_mutex_t* mutex)
{
    return (0 == pthread_mutex_lock(mutex));
}
int se_mutex_unlock(se_mutex_t* mutex)
{
    return (0 == pthread_mutex_unlock(mutex));
}
int se_mutex_destroy(se_mutex_t* mutex)
{
    return (0 == pthread_mutex_destroy(mutex));
}

/*
 * thread
 */
void se_thread_cond_init(se_cond_t* cond)
{
    se_cond_t tmp = PTHREAD_COND_INITIALIZER;
    memcpy(cond, &tmp, sizeof(tmp));
}

int se_thread_cond_wait(se_cond_t *cond, se_mutex_t *mutex)
{
    return (0 == pthread_cond_wait(cond, mutex));
}
int se_thread_cond_signal(se_cond_t *cond)
{
    return (0 == pthread_cond_signal(cond));
}
int se_thread_cond_broadcast(se_cond_t *cond)
{
    return (0 == pthread_cond_broadcast(cond));
}
int se_thread_cond_destroy(se_cond_t* cond)
{
    return (0 == pthread_cond_destroy(cond));
}

/*
 * get_threadid
 */
unsigned int se_get_threadid(void)
{
    return (unsigned)syscall(__NR_gettid);
}
