#ifndef _MOUSE_H_
#define _MOUSE_H_
#include "aesm_long_lived_thread.h"
#include <string>
#include "aeerror.h"

class MouseIOCache:public BaseThreadIOCache {
public:
    MouseIOCache(){}
    MouseIOCache(char const * name):_name(name) {}
    virtual ae_error_t entry();
    virtual ThreadStatus& get_thread();
    friend ae_error_t start_mouse_thread(unsigned long timeout);
    virtual bool operator==(const BaseThreadIOCache& oc)const {
        const MouseIOCache *p = dynamic_cast<const MouseIOCache *>(&oc);
        if(p==NULL) return false;
        return true;
    }
private:
    std::string _name;

};
#endif
