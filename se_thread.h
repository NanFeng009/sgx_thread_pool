#include <pthread.h>

/*
 * This file is to wrapper thread related API that is different from windows to linux.
 * */
#ifndef _SE_THREAD_H_
#define _SE_THREAD_H_

typedef pthread_mutex_t se_mutex_t;
typedef pthread_cond_t se_cond_t;
typedef pid_t se_thread_id_t;
typedef pthread_key_t se_tls_index_t;


#ifdef __cplusplus
extern "C" {
#endif
/*
@mutex: A pointer to the critical section object.
@return value:  If the function succeeds, the return value is nonzero.If the function fails, the return value is zero.
*/
void se_mutex_init(se_mutex_t* mutex);
int se_mutex_lock(se_mutex_t* mutex);
int se_mutex_unlock(se_mutex_t* mutex);
int se_mutex_destroy(se_mutex_t* mutex);

void se_thread_cond_init(se_cond_t* cond);
int se_thread_cond_wait(se_cond_t *cond, se_mutex_t *mutex);
int se_thread_cond_signal(se_cond_t *cond);
int se_thread_cond_broadcast(se_cond_t *cond);
int se_thread_cond_destroy(se_cond_t* cond);

unsigned int se_get_threadid(void);

#ifdef __cplusplus
}
#endif



#endif
