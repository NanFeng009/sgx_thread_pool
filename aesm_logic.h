#ifndef _AESM_LOGIC_H_
#define _AESM_LOGIC_H_
#include <stdint.h>
#include "se_thread.h"
#include <pthread.h>

const uint32_t THREAD_TIMEOUT = 5000;

#define CLASS_UNCOPYABLE(classname) \
    private: \
    classname(const classname&); \
    classname& operator=(const classname&);

typedef pthread_mutex_t se_mutex_t;

class AESMLogicMutex {
    CLASS_UNCOPYABLE(AESMLogicMutex)
public:
    AESMLogicMutex() {
        se_mutex_init(&mutex);
    }
    ~AESMLogicMutex() {
        se_mutex_destroy(&mutex);
    }
    void lock() {
        se_mutex_lock(&mutex);
    }
    void unlock() {
        se_mutex_unlock(&mutex);
    }
private:
    se_mutex_t mutex;
};

class AESMLogicLock {
    CLASS_UNCOPYABLE(AESMLogicLock)
public:
    explicit AESMLogicLock(AESMLogicMutex& cs) :_cs(cs) {
        _cs.lock();
    }
    ~AESMLogicLock() {
        _cs.unlock();
    }
private:
    AESMLogicMutex& _cs;
};
#endif
