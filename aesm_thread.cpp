#include "aesm_thread.h"
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include "se_thread.h"//using se_mutex
#include <stdio.h>
#include <string.h>

const uint32_t AESM_THREAD_INFINITE = 0xffffffff; // special flag to indicate that thread to be blocked

//thread status:
//  At the beginning, status is initialized to AESM_THREAD_INIT
//    After the started thread has copied all input data, the status will be updated to AESM_THREAD_RUNNING (So that aesm_free_thread could be called and mainthread could quit)
//    If the thread has finished before function like aesm_free_thread is called, the thread will find the status is still AESM_TRHEAD_RUNNING,
//        it will update the status to AESM_THREAD_PENDING and leave the resource free to following call of aesm_free_thread
//    If the thread is finished after function aesm_free_thread is called, the status should be AESM_THREAD_FREED, so that it should free the resource.
//    If aesm_join_thread is called, it will wait the  thread to be finished (so that it should find the status to be AESM_THREAD_PENDING when pthread_join returned
//        and it will update the status to AESM_THREAD_DETACHED
//    If aesm_free_thread is called before the thread has been finished, so the function find the status is AESM_THREAD_RUNNING,
//       it will update the status to AESM_THREAD_FREED and leaving the memory free the aesm_thread_proc
//    But when aesm_free_thread is called and it found the thread has been finished so that the status is AESM_THREAD_PENDING or AESM_THREAD_DETACHED
//     the function should free all the resource.
enum _aesm_thread_status_t {AESM_THREAD_INIT,  AESM_THREAD_INVALID, AESM_THREAD_RUNNING, AESM_THREAD_PENDING, AESM_THREAD_DETACHED, AESM_THREAD_FREED};


struct _aesm_thread_t {
    pthread_mutex_t mutex;
    pthread_cond_t  copy_cond;
    pthread_cond_t  timeout_cond;
    aesm_thread_arg_type_t arg;
    aesm_thread_function_t fun_entry;
    ae_error_t      ae_ret;
    pthread_t hthread;
    volatile _aesm_thread_status_t status;
};

static void aesm_dealloc_resource(aesm_thread_t h)
{
    h->status = AESM_THREAD_INVALID;
    pthread_cond_destroy(&h->copy_cond);
    pthread_cond_destroy(&h->timeout_cond);
    (void)pthread_mutex_destroy(&h->mutex);
    free(h);
}

void* aesm_thread_proc(void* param)
{
    struct _aesm_thread_t *p=(struct _aesm_thread_t *)param;
    aesm_thread_function_t fun_entry = NULL;
    aesm_thread_arg_type_t arg = 0;
    printf("start running thread %p...\n",param);
    if(pthread_mutex_lock(&p->mutex)!=0) {
        printf("fail to lock the thread mutex of thread %p\n",param);
        return reinterpret_cast<void *>(static_cast<ptrdiff_t>(AE_FAILURE));
    }
    fun_entry = p->fun_entry;
    arg = p->arg;
    p->status = AESM_THREAD_RUNNING;
    p->ae_ret = AE_FAILURE;
    pthread_cond_signal(&p->copy_cond);//notify mainthread that input parameter data has been copied to thread and we could release it(if aesm_thread_free has been called)
    pthread_mutex_unlock(&p->mutex);
    printf("thread parameters of thread %p copied\n",param);
    ae_error_t err = fun_entry(arg);
    printf("returned from user defined thread code for thread %p\n",param);
    if(pthread_mutex_lock(&p->mutex)!=0) {
        p->status = AESM_THREAD_INVALID;
        printf("fail to lock the thread mutex of thread %p\n",param);
        return reinterpret_cast<void *>(static_cast<ptrdiff_t>(AE_FAILURE));
    }
    p->ae_ret = err;
    if(p->status == AESM_THREAD_RUNNING) {
        p->status = AESM_THREAD_PENDING;
        pthread_cond_signal(&p->timeout_cond);
        pthread_mutex_unlock(&p->mutex);
        printf("thread %p change to status AESM_THREAD_PEDNING\n",param);
    } else if(p->status == AESM_THREAD_FREED) {
        pthread_mutex_unlock(&p->mutex);
        pthread_detach(p->hthread);
        aesm_dealloc_resource(p);
        printf("resource of thread %p has been dealloced\n",param);
    } else {
        p->status = AESM_THREAD_INVALID;//It should never be reached
        pthread_mutex_unlock(&p->mutex);
        printf("thread %p status invalid\n",param);
        assert(0);
    }
    return reinterpret_cast<void *>(static_cast<ptrdiff_t>(err));
}

ae_error_t aesm_create_thread(aesm_thread_function_t function_entry, aesm_thread_arg_type_t arg, aesm_thread_t* h)
{
    ae_error_t err = AE_SUCCESS;
    int r=0;
    int mutex_inited = 0;
    int copy_cond_inited = 0;
    int timeout_cond_inited = 0;
    assert(h!=NULL);
    printf("start to create a thread\n");
    struct _aesm_thread_t *p=(struct _aesm_thread_t *)malloc(sizeof(struct _aesm_thread_t));
    if(p==NULL) {
        printf("fail to malloc\n");
        err = AE_OUT_OF_MEMORY_ERROR;
        goto ret_point;
    }
    memset(p, 0, sizeof(struct  _aesm_thread_t));
    p->arg = arg;
    p->fun_entry = function_entry;
    if(pthread_mutex_init(&p->mutex, NULL)!=0) {
        err = AE_FAILURE;
        printf("fail to init mutex\n");
        goto ret_point;
    }
    mutex_inited=1;
    if(pthread_cond_init(&p->copy_cond, NULL)!=0) {
        err = AE_FAILURE;
        printf("fail to init copy cond\n");
        goto ret_point;
    }
    copy_cond_inited=1;
    if(pthread_cond_init(&p->timeout_cond, NULL)!=0) {
        err = AE_FAILURE;
        printf("fail to init timeout cond\n");
        goto ret_point;
    }
    timeout_cond_inited=1;
    p->status  = AESM_THREAD_INIT;
    r=pthread_create(&p->hthread, NULL, aesm_thread_proc, p);
    if(r!=0) {
        printf("fail to create thread\n");
        err = OAL_THREAD_ERROR;
        goto ret_point;
    }
    *h = p;
    printf("thread %p created successfully\n",p);
ret_point:
    if(err!=AE_SUCCESS&&p!=NULL) {
        if(mutex_inited!=0) {
            (void)pthread_mutex_destroy(&p->mutex);
        }
        if(copy_cond_inited!=0) {
            (void)pthread_cond_destroy(&p->copy_cond);
        }
        if(timeout_cond_inited!=0) {
            (void)pthread_cond_destroy(&p->timeout_cond);
        }
        free(p);
    }
    return err;
}

ae_error_t aesm_join_thread(aesm_thread_t h, ae_error_t *thread_ret)
{
    void *ret_value;
    printf("start to join thread %p\n",h);
    if (h == NULL)
    {
        printf("Thread handle is NULL.\n");
        return OAL_THREAD_ERROR;
    }
    if(0!=pthread_join(h->hthread, &ret_value)) {
        printf("fail to join thread %p\n",h);
        return OAL_THREAD_ERROR;
    }
    if(pthread_mutex_lock(&h->mutex)!=0) {
        printf("fail to lock thread %p\n",h);
        return AE_FAILURE;
    }
    if(h->status != AESM_THREAD_PENDING) {
        printf("thread %p status error %d in join\n",h,h->status);
        h->status = AESM_THREAD_INVALID;//invalid
        pthread_mutex_unlock(&h->mutex);
        assert(0);
        return OAL_THREAD_ERROR;
    }
    h->status = AESM_THREAD_DETACHED;//no call to pthread_detach after pthread_join
    pthread_mutex_unlock(&h->mutex);
    *thread_ret = static_cast<ae_error_t>(reinterpret_cast<ptrdiff_t>(ret_value));
    printf("thread %p join successfully with return value %d\n",h,*thread_ret);
    return AE_SUCCESS;
}

ae_error_t aesm_free_thread(aesm_thread_t h)
{
    if (h == NULL) {
        return AE_SUCCESS;
    }

    printf("start to free thread %p\n",h);
    if(pthread_mutex_lock(&h->mutex)!=0) {
        printf("fail to lock thread %p\n",h);
        return AE_FAILURE;
    }
    if(h->status == AESM_THREAD_INIT) { //wait for parameters copy to be finished in thread_proc
        printf("wait for parameter copy in thread %p\n",h);
        (void)pthread_cond_wait(&h->copy_cond, &h->mutex);//ignore the waiting error, we will continue to free the thread info even if waiting failed anyway
        //The h->status should have been updated if the waiting of copy_cond is finished
    }
    if(h->status == AESM_THREAD_RUNNING) {
        h->status = AESM_THREAD_FREED;
    } else if(h->status == AESM_THREAD_PENDING) {
        h->status = AESM_THREAD_DETACHED;
        pthread_detach(h->hthread);
        printf("thread %p detached\n",h);
    }
    if(h->status == AESM_THREAD_DETACHED) {
        pthread_mutex_unlock(&h->mutex);
        aesm_dealloc_resource(h);
        printf("thread %p resource dealloced\n",h);
        return AE_SUCCESS;
    } else if(h->status != AESM_THREAD_FREED) {
        pthread_mutex_unlock(&h->mutex);
        printf("thread %p status error %d\n",h,h->status);
        aesm_dealloc_resource(h);//dealloc anyway
        assert(0);
        return OAL_THREAD_ERROR;
    } else {
        pthread_mutex_unlock(&h->mutex);
        printf("thread %p marked to be free\n",h);
        return AE_SUCCESS;
    }
}


ae_error_t aesm_wait_thread(aesm_thread_t h, ae_error_t *thread_ret, unsigned long milisecond)
{
    printf("aesm_wait_thread :start to wait thread %p for %ld ms\n",h,milisecond);
    if (h == NULL)
    {
        printf("aesm_wait_thread : Thread handle is NULL.\n");
        return OAL_THREAD_ERROR;
    }
    if(pthread_mutex_lock(&h->mutex)!=0) {
        printf("aesm_wait_thread : Fail to hold lock of thread %p\n",h);
        return OAL_THREAD_ERROR;
    }
    if(h->status==AESM_THREAD_PENDING||h->status==AESM_THREAD_DETACHED) { //if the thread has been finished
        pthread_mutex_unlock(&h->mutex);
        printf("aesm_wait_thread : thread %p is pending\n",h);
        return AE_SUCCESS;
    } else if(h->status!=AESM_THREAD_INIT&&h->status!=AESM_THREAD_RUNNING) {
        pthread_mutex_unlock(&h->mutex);
        printf("aesm_wait_thread : invalid thread status %d for thread %p\n",h->status,h);
        return OAL_THREAD_ERROR;
    }
    struct timespec abstime;
    clock_gettime(CLOCK_REALTIME, &abstime);
    abstime.tv_sec += milisecond/1000 + (abstime.tv_nsec/1000000+milisecond%1000)/1000;
    abstime.tv_nsec = (abstime.tv_nsec/1000000+milisecond%1000)%1000 *1000000 + abstime.tv_nsec%1000000;
    //! struct timeval now;
    //! gettimeofday(&now, NULL);
    //! long int usec = now.tv_usec + (milisecond % 1000)*1000000;
    //! abstime.tv_nsec = (usec % 1000000) *1000;
    //! abstime.tv_sec = now.tv_sec + usec / 1000000  + milisecond / 1000;

    printf("aesm_wait_thread : start to pthread_cond_wait\n");
    int err_no;
    err_no = pthread_cond_timedwait(&h->timeout_cond, &h->mutex, &abstime);
    if(err_no == ETIMEDOUT)
    {
        pthread_mutex_unlock(&h->mutex);
        printf("aesm_wait_thread : thread %p waiting timeout\n",h);
        return OAL_THREAD_TIMEOUT_ERROR;
    }
    else if(err_no == 0)
    {
        *thread_ret = h->ae_ret;
        pthread_mutex_unlock(&h->mutex);
        printf("aesm_wait_thread : thread %p is detached with return value %d\n",h,*thread_ret);
        return AE_SUCCESS;
    }
    else
    {
        pthread_mutex_unlock(&h->mutex);
        printf("aesm_wait_thread : thread wait error in thread %p\n",h);
        return OAL_THREAD_ERROR;
    }
    printf("aesm_wait_thread : leave aesm_wait_thread\n");
}
