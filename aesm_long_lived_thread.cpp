#include "uncopyable.h"
#include "aesm_long_lived_thread.h"
#include "mouse.h"
#include "animal_info_logic.h"
#include "se_time.h"

ae_error_t BaseThreadIOCache::start(BaseThreadIOCache *&out_ioc, uint32_t timeout_value)
{
    return get_thread().set_thread_start(this, out_ioc, timeout_value);
}

void BaseThreadIOCache::deref(void)
{
    get_thread().deref(this);
}

void BaseThreadIOCache::set_status_finish(void)
{
    get_thread().set_status_finish(this);
}

/* below are function for ThreadStatus */
//This is thread entry wrapper for all threads
static ae_error_t aesm_long_lived_thread_entry(aesm_thread_arg_type_t arg)
{
    BaseThreadIOCache *cache = (BaseThreadIOCache *)arg;
    printf("dashuaic in %s **************************** the address of cache = %p, and the status = %d\n",__FUNCTION__, cache, cache->status);
    ae_error_t ae_err = cache->entry();
    printf("dashuaic in %s **************************** the address of cache = %p, and the status = %d\n", __FUNCTION__, cache, cache->status);
    cache->set_status_finish();
    return ae_err;
}
ae_error_t ThreadStatus::set_thread_start(BaseThreadIOCache* ioc, BaseThreadIOCache *&out_ioc, uint32_t timeout)
{
    ae_error_t ae_ret = AE_SUCCESS;
    ae_error_t ret = AE_FAILURE;
    out_ioc=NULL;
    bool fork_required = find_or_insert_iocache(ioc, out_ioc);
    printf("dashuaic in %s ************************** the address of out_ioc = %p, and the status = %d\n", __FUNCTION__, out_ioc, out_ioc->status);
    if(fork_required) {
        ae_ret = aesm_create_thread(aesm_long_lived_thread_entry, (aesm_thread_arg_type_t)out_ioc, &out_ioc->thread_handle);
    printf("dashuaic in 2  %s ************************** the address of out_ioc = %p, and the status = %d\n", __FUNCTION__, out_ioc, out_ioc->status);
        if (ae_ret != AE_SUCCESS)
        {
            printf("fail to create thread for ioc %p\n",out_ioc);
            AESMLogicLock locker(thread_mutex);
            thread_state = ths_idle;
            out_ioc->status = ioc_idle;//set to finished status
            cur_iocache = NULL;
            deref(out_ioc);
            return ae_ret;
        } else {
            printf("succ create thread %p for ioc %p\n",this, out_ioc);
        }
    }

    if(out_ioc == NULL) {
        printf("no ioc created for input ioc %p in thread %p\n",ioc, this);
        return OAL_THREAD_TIMEOUT_ERROR;
    }

    {   //check whether thread has been finished
        AESMLogicLock locker(thread_mutex);
        if(out_ioc->status!=ioc_busy) { //job is done
            printf("job done for ioc %p (status=%d,timeout=%d,ref_count=%d) in thread %p\n",out_ioc, (int)out_ioc->status,(int)out_ioc->timeout,(int)out_ioc->ref_count,this);
            return AE_SUCCESS;
        }
    }

    if(timeout >= AESM_THREAD_INFINITE ) {
        ae_ret = aesm_join_thread(out_ioc->thread_handle, &ret);
    } else {
        uint64_t now = se_get_tick_count();
        double timediff = static_cast<double>(timeout) - (static_cast<double>(now - status_clock))/static_cast<double>(se_get_tick_count_freq()) *1000;
        if (timediff <= 0.0) {
            printf("long flow thread timeout\n");
            return OAL_THREAD_TIMEOUT_ERROR;
        }
        else {
            printf("timeout:%u,timediff: %f\n", timeout,timediff);
            ae_ret = aesm_wait_thread(out_ioc->thread_handle, &ret, (unsigned long)timediff);
        }
    }
    printf("wait for ioc %p (status=%d,timeout=%d,ref_count=%d) result:%d\n",out_ioc,(int)out_ioc->status,(int)out_ioc->timeout,(int)out_ioc->ref_count, ae_ret);
    return ae_ret;
};

void ThreadStatus::stop_thread(uint64_t stop_tick_count)
{
    //change state to stop
    thread_mutex.lock();
    thread_state = ths_stop;

    do {
        std::list<BaseThreadIOCache *>::iterator it;
        for(it=output_cache.begin(); it!=output_cache.end(); ++it) {
            BaseThreadIOCache *p=*it;
            if(p->status != ioc_stop) { //It has not been processed
                printf("here set the status to ioc_stop\n");
                p->status = ioc_stop;
                break;
            }
        }
        if(it!=output_cache.end()) { //found item to stop
            BaseThreadIOCache *p = *it;
            p->ref_count++;
            thread_mutex.unlock();
            wait_iocache_timeout(p, stop_tick_count);
            thread_mutex.lock();
        } else {
            break;
        }
    } while(1);

    thread_mutex.unlock();
    //This function should only be called at AESM exit
    //Leave memory leak here is OK and all pointer to BaseThreadIOCache will not be released
}

ae_error_t ThreadStatus::wait_for_cur_thread(uint64_t millisecond)
{
    BaseThreadIOCache *ioc = NULL;
    uint64_t stop_tick_count;
    if(millisecond == AESM_THREAD_INFINITE) {
        stop_tick_count = THREAD_INFINITE_TICK_COUNT;
    } else {
        stop_tick_count = se_get_tick_count() + (millisecond*se_get_tick_count_freq())/1000;
    }
        printf("wait_for_cur_thread  ioc %p(refcount=%d), and status = %d\n",ioc,ioc->ref_count, ioc->status);
    thread_mutex.lock();
    if(cur_iocache != NULL) {
        ioc = cur_iocache;
        ioc->ref_count++;
        printf("wait_for_cur_thread  ioc %p(refcount=%d), and status = %d\n",ioc,ioc->ref_count, ioc->status);
    }
    thread_mutex.unlock();
    if(ioc!=NULL) {
        return wait_iocache_timeout(ioc, stop_tick_count);
    }
    return AE_SUCCESS;
}

ae_error_t ThreadStatus::wait_iocache_timeout(BaseThreadIOCache* ioc, uint64_t stop_tick_count)
{
    ae_error_t ae_ret = AE_SUCCESS;
    uint64_t cur_tick_count = se_get_tick_count();
    uint64_t freq = se_get_tick_count_freq();
    bool need_wait = false;
    aesm_thread_t handle = NULL;
        printf("wait for busy -1  ioc %p(refcount=%d), and status = %d\n",ioc,ioc->ref_count, ioc->status);
    thread_mutex.lock();
    if(ioc->thread_handle!=NULL&&(cur_tick_count<stop_tick_count||stop_tick_count==THREAD_INFINITE_TICK_COUNT)) {
        printf("wait for busy ioc %p(refcount=%d), and status = %d\n",ioc,ioc->ref_count, ioc->status);
        need_wait = true;
        handle = ioc->thread_handle;
    }
    thread_mutex.unlock();
    if(need_wait) {
        unsigned long diff_time;
        if(stop_tick_count == THREAD_INFINITE_TICK_COUNT) {
            diff_time = AESM_THREAD_INFINITE;
        } else {
            double wtime=(double)(stop_tick_count-cur_tick_count)*1000.0/(double)freq;
            diff_time = (unsigned long)(wtime+0.5);
        }
        ae_ret= aesm_wait_thread(handle, &ae_ret, diff_time);
    }
    deref(ioc);
    return ae_ret;
}


bool ThreadStatus::query_status_and_reset_clock()
{
    AESMLogicLock locker(thread_mutex);
    if(thread_state == ths_busy || thread_state == ths_stop)
        return false;
    status_clock = se_get_tick_count();
    return true;
}

#define TIMEOUT_SHORT_TIME    60
#define TIMEOUT_FOR_A_WHILE   (5*60)
#define TIMEOUT_LONG_TIME     (3600*24) //at most once every day
static time_t get_timeout_via_ae_error(ae_error_t ae)
{
    time_t cur=time(NULL);
    switch(ae) {
    case AE_SUCCESS:
    case OAL_PROXY_SETTING_ASSIST:
    case OAL_NETWORK_RESEND_REQUIRED:
        return cur-1;//always timeout, the error code will never be reused
    case PVE_INTEGRITY_CHECK_ERROR:
#ifndef SGX_DISABLE_PSE
    case PSE_OP_ERROR_EPH_SESSION_ESTABLISHMENT_INTEGRITY_ERROR:
    case AESM_PSDA_LT_SESSION_INTEGRITY_ERROR:
#endif
    case OAL_NETWORK_UNAVAILABLE_ERROR:
    case OAL_NETWORK_BUSY:
    case PVE_SERVER_BUSY_ERROR:
        return cur+TIMEOUT_SHORT_TIME; //retry after short time
    case QE_REVOKED_ERROR:
    case PVE_REVOKED_ERROR:
    case PVE_MSG_ERROR:
    case PVE_PERFORMANCE_REKEY_NOT_SUPPORTED:
#ifndef SGX_DISABLE_PSE
    case AESM_PSDA_PLATFORM_KEYS_REVOKED:
    case AESM_PSDA_PROTOCOL_NOT_SUPPORTED:
#endif
    case PSW_UPDATE_REQUIRED:
        return cur+TIMEOUT_LONG_TIME;
    default:
        return cur+TIMEOUT_SHORT_TIME;//retry quicky for unknown error
    }
}


void ThreadStatus::set_status_finish(BaseThreadIOCache* ioc)
{
    aesm_thread_t handle = NULL;
    {
        AESMLogicLock locker(thread_mutex);
        assert(thread_state==ths_busy||thread_state==ths_stop);
    printf("dashuaic in %s  **************************** the address of ioc= %p, and the status = %d\n", __FUNCTION__, ioc, ioc->status);
        assert(ioc->status == ioc_busy);
        printf("set finish status for ioc %p(status=%d,timeout=%d,ref_count=%d) of thread %p\n",ioc, (int)ioc->status,(int)ioc->timeout,(int)ioc->ref_count,this);
        if(thread_state==ths_busy) {
            printf("set thread %p to idle\n", this);
            thread_state=ths_idle;
            cur_iocache = NULL;
        }
        ioc->status=ioc_idle;
        ioc->ref_count--;
        ioc->timeout = get_timeout_via_ae_error(ioc->ae_ret);
        if(ioc->ref_count==0) { //try free thread handle
            handle = ioc->thread_handle;
            ioc->thread_handle = NULL;
            printf("thread handle release for ioc %p and status to idle of thread %p\n",ioc, this);
        }
    }
    if(handle!=NULL) {
        aesm_free_thread(handle);
    }
}




void ThreadStatus::deref(BaseThreadIOCache *ioc)
{
    aesm_thread_t handle = NULL;
    time_t cur=time(NULL);
    {
        AESMLogicLock locker(thread_mutex);
        printf("deref ioc %p (ref_count=%d,status=%d,timeout=%d) of thread %p\n",ioc,(int)ioc->ref_count,(int)ioc->status,(int)ioc->timeout, this);
        --ioc->ref_count;
        if(ioc->ref_count == 0) { //try free the thread handle now
            handle = ioc->thread_handle;
            ioc->thread_handle = NULL;
            if(ioc->status == ioc_busy) {
                ioc->status = ioc_idle;
            }
            printf("free thread handle for ioc %p\n",ioc);
        }
        if(ioc->ref_count==0 &&(ioc->status==ioc_stop||ioc->timeout<cur)) {
            printf("free ioc %p\n",ioc);
            output_cache.remove(ioc);
            printf("thread %p cache's size is %d\n",this, (int)output_cache.size());
            delete ioc;
        }
    }
    if(handle!=NULL) {
        aesm_free_thread(handle);
    }
        printf("dashuaic in deref  deref ioc %p (ref_count=%d,status=%d,timeout=%d) of thread %p\n",ioc,(int)ioc->ref_count,(int)ioc->status,(int)ioc->timeout, this);
}
////start implementation of external functions
//
//#define INIT_THREAD(cache_type, timeout, init_list) \
//    BaseThreadIOCache *ioc = new cache_type init_list; \
//    BaseThreadIOCache *out_ioc = NULL; \
//    ae_error_t ae_ret = AE_FAILURE; \
//    ae_ret = ioc->start(out_ioc, (uint32_t)(timeout)); \
//    if(ae_ret != AE_SUCCESS){ \
//        if(out_ioc!=NULL){out_ioc->deref();}\
//        return ae_ret; \
//    }\
//    assert(out_ioc!=NULL);\
//    cache_type *pioc = dynamic_cast<cache_type *>(out_ioc);\
//    assert(pioc!=NULL);
////now the thread has finished it's execution and we could read result without lock
//#define COPY_OUTPUT(x)  x=pioc->x
//#define FINI_THREAD() \
//    ae_ret = pioc->ae_ret;\
//    pioc->deref();/*derefence the cache object after usage of it*/ \
//    return ae_ret;
//
////usage model
////INIT_THREAD(thread_used, cache_type, timeout, init_list)
//// COPY_OUTPUT(is_new_pairing);// copy out output parameter except for return value from pioc object to output parameter, such as
////FINI_THREAD(thread_used)


static ThreadStatus eat_thread;
static ThreadStatus sleep_thread;
#if 0
class eatIOCache:public BaseThreadIOCache {
  public:
    eatIOCache() {};
    virtual ae_error_t entry();
    virtual ThreadStatus& get_thread();
    friend ae_error_t start_eat_thread(unsigned long timeout);
    virtual bool operator==(const BaseThreadIOCache& oc)const {
        const eatIOCache *p = dynamic_cast<const eatIOCache *>(&oc);
        if(p==NULL) return false;
        return true;
    }
};

class sleepIOCache:public BaseThreadIOCache {
  public:
    sleepIOCache() {};
    virtual ae_error_t entry();
    virtual ThreadStatus& sleep_thread();
    friend ae_error_t start_sleep_thread(unsigned long timeout);
    virtual bool operator==(const BaseThreadIOCache& oc)const {
        const sleepIOCache *p = dynamic_cast<const sleepIOCache *>(&oc);
        if(p==NULL) return false;
        return true;
    }
};

#endif


// definition for eatIOCache
ae_error_t eatIOCache::entry()
{
    printf("enter eatIOCache the status = %d\n", this->status);
    ae_ret = AnimalInfoLogic::check_eat_thread_func();
    printf("leave  eatIOCache the status = %d\n", this->status);
    return ae_ret;
}
ThreadStatus& eatIOCache::get_thread()
{
    return eat_thread;
}

ae_error_t start_eat_thread(unsigned long timeout)
{
    INIT_THREAD(eatIOCache, timeout, ())
    //COPY_OUTPUT(false);
    FINI_THREAD()
}


//
bool query_eat_thread_status()
{
    return eat_thread.query_status_and_reset_clock();
}


