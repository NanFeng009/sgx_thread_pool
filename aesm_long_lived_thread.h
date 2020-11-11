#ifndef _AESM_LONG_LIVED_THREAD_H_
#define _AESM_LONG_LIVED_THREAD_H_
#include "uncopyable.h"
#include <time.h>
#include "aeerror.h"
#include <stdio.h>
#include <stdint.h>
#include "aesm_logic.h"
#include <list>
#include <assert.h>
#include "aesm_thread.h"

#define MAX_OUTPUT_CACHE 50
#define THREAD_INFINITE_TICK_COUNT 0xFFFFFFFFFFFFFFFFLL

enum _thread_state {
    ths_idle,
    ths_busy,
    ths_stop//The thread is to be stopped and no new job will be accepted
};

//const uint32_t THREAD_TIMEOUT = 5000;
enum _io_cache_state {
    ioc_idle, //thread has been finished
    ioc_busy, //thread not finished yet
    ioc_stop  //thread stop required
};


//start implementation of external functions

#define INIT_THREAD(cache_type, timeout, init_list) \
    BaseThreadIOCache *ioc = new cache_type init_list; \
    BaseThreadIOCache *out_ioc = NULL; \
    ae_error_t ae_ret = AE_FAILURE; \
    ae_ret = ioc->start(out_ioc, (uint32_t)(timeout)); \
    if(ae_ret != AE_SUCCESS){ \
        if(out_ioc!=NULL){out_ioc->deref();}\
        return ae_ret; \
    }\
    assert(out_ioc!=NULL);\
    cache_type *pioc = dynamic_cast<cache_type *>(out_ioc);\
    assert(pioc!=NULL);
//now the thread has finished it's execution and we could read result without lock
#define COPY_OUTPUT(x)  x=pioc->x
#define FINI_THREAD() \
    ae_ret = pioc->ae_ret;\
    pioc->deref();/*derefence the cache object after usage of it*/ \
    return ae_ret;

//usage model
//INIT_THREAD(thread_used, cache_type, timeout, init_list)
// COPY_OUTPUT(is_new_pairing);// copy out output parameter except for return value from pioc object to output parameter, such as
//FINI_THREAD(thread_used)


typedef struct _aesm_thread_t *aesm_thread_t;

class ThreadStatus;
class BaseThreadIOCache;
//Base class for cached data of each thread to fork
class BaseThreadIOCache:private Uncopyable {
    time_t timeout; //The data will timeout after the time if the state is not busy
    int ref_count; //ref_count is used to track how many threads are currently referencing the data
    public:
    _io_cache_state status;
    //handle of the thread, some thread will be waited by other threads so that we could not
    //   free the handle until all other threads have got notification that the thread is terminated
    aesm_thread_t thread_handle;
    friend class ThreadStatus;
protected:
    ae_error_t ae_ret;
    BaseThreadIOCache():ref_count(0),status(ioc_busy) {
        timeout=0;
        thread_handle=NULL;
        ae_ret = AE_FAILURE;
    }
    virtual ThreadStatus& get_thread()=0;
public:
    virtual ae_error_t entry(void)=0;
    virtual bool operator==(const BaseThreadIOCache& oc)const=0;
    ae_error_t start(BaseThreadIOCache *&out_ioc, uint32_t timeout=THREAD_TIMEOUT);
    void deref(void);
    void set_status_finish();
public:
    virtual ~BaseThreadIOCache() {}
};

class ThreadStatus: private Uncopyable {
private:
    AESMLogicMutex thread_mutex;
    _thread_state thread_state;
    uint64_t    status_clock;
    BaseThreadIOCache *cur_iocache;
    std::list<BaseThreadIOCache *>output_cache;
protected:
    //function to look up cached output, there will be no real thread associated with the input ioc
    //If a match is found the input parameter will be free automatically and the matched value is returned
    //return true if a thread will be forked for the out_ioc
    bool find_or_insert_iocache(BaseThreadIOCache* ioc, BaseThreadIOCache *&out_ioc) {
        AESMLogicLock locker(thread_mutex);
        std::list<BaseThreadIOCache *>::reverse_iterator it;
        out_ioc=NULL;
        if(thread_state == ths_stop) {
            printf("thread %p has been stopped and ioc %p not inserted\n", this,ioc);
            delete ioc;
            return false;//never visit any item after thread is stopped
        }
        time_t cur=time(NULL);
        printf("cache size %d\n",(int)output_cache.size());
        BaseThreadIOCache *remove_candidate = NULL;
        for(it=output_cache.rbegin(); it!=output_cache.rend(); ++it) { //visit the cache in reverse order so that the newest item will be visited firstly
            BaseThreadIOCache *pioc=*it;
            if((pioc->status==ioc_idle)&&(pioc->timeout<cur)) {
                if(pioc->ref_count==0&&remove_candidate==NULL) {
                    remove_candidate = pioc;
                }
                continue;//value timeout
            }
            if(*pioc==*ioc) { //matched value find
                pioc->ref_count++;//reference it
                printf("IOC %p matching input IOC %p (ref_count:%d,status:%d,timeout:%d) in thread %p\n",pioc, ioc,(int)pioc->ref_count,(int)pioc->status, (int)pioc->timeout, this);
                out_ioc= pioc;
                delete ioc;
                return false;
            }
        }
        if(thread_state == ths_busy) { //It is not permitted to insert in busy status
            printf("thread busy when trying insert input ioc %p\n",ioc);
            delete ioc;
            return false;
        }
        if(remove_candidate!=NULL) {
            output_cache.remove(remove_candidate);
            delete remove_candidate;
        }
        if(output_cache.size()>=MAX_OUTPUT_CACHE) {
            std::list<BaseThreadIOCache *>::iterator fit;
            bool erased=false;
            for(fit = output_cache.begin(); fit!=output_cache.end(); ++fit) {
                BaseThreadIOCache *pioc=*fit;
                if(pioc->ref_count==0) { //find a not timeout item to remove
                    assert(pioc->status==ioc_idle);
                    printf("erase idle ioc %p", pioc);
                    output_cache.erase(fit);
                    erased = true;
                    printf("thread %p cache size %ld\n",this, output_cache.size());
                    delete pioc;
                    break;
                }
            }
            if(!erased) { //no item could be removed
                printf("no free ioc found and cannot insert ioc %p\n",ioc);
                delete ioc;
                return false;//similar as busy status
            }
        }
        output_cache.push_back(ioc);
        out_ioc = cur_iocache = ioc;
        cur_iocache->ref_count=2;//initialize to be refenced by parent thread and the thread itself
        thread_state = ths_busy;//mark thread to be busy that the thread to be started
        printf("successfully add ioc %p (status=%d,timeout=%d) into thread %p\n",out_ioc, (int)out_ioc->status, (int)out_ioc->timeout, this);
        int i = 10;
        printf("prepare to set \n");
        while( i < 100){
            i++; };
        printf("shoud set successfully\n");
        return true;
    }

public:
    ThreadStatus():output_cache() {
        thread_state = ths_idle;
        status_clock = 0;
        cur_iocache = NULL;
    }
    void set_status_finish(BaseThreadIOCache* ioc);//only called at the end of aesm_long_lived_thread_entry
    void deref(BaseThreadIOCache* iocache);
    ae_error_t wait_iocache_timeout(BaseThreadIOCache* ioc, uint64_t stop_tick_count);

    //create thread and wait at most 'timeout' for the thread to be finished
    // It will first look up whether there is a previous run with same input before starting the thread
    // we should not delete ioc after calling to this function
    ae_error_t set_thread_start(BaseThreadIOCache* ioc,  BaseThreadIOCache *&out_ioc, uint32_t timeout=THREAD_TIMEOUT);

    void stop_thread(uint64_t stop_milli_second);//We need wait for thread to be terminated and all thread_handle in list to be closed

    ~ThreadStatus() {
        printf("start to deconstructor ThreadStatus\n");
        stop_thread(THREAD_INFINITE_TICK_COUNT);   //ThreadStatus instance should be global object. Otherwise, it is possible that the object is destroyed before a thread waiting for and IOCache got notified and causing exception
    }

    ae_error_t wait_for_cur_thread(uint64_t millisecond);

    //function to query whether current thread is idle,
    //if it is idle, return true and reset clock to current clock value
    bool query_status_and_reset_clock(void);
};

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

extern bool query_eat_thread_status();
extern ae_error_t start_eat_thread(unsigned long timeout);
#endif
