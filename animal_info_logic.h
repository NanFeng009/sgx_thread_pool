#ifndef _ANIMAL_INFO_LOGIC_H_
#define _ANIMAL_INFO_LOGIC_H_

#include "aeerror.h"

/* File to declare AnimalInfoLogic Class which is animal independent */
class AnimalInfoLogic {
public:
    static ae_error_t check_eat_thread_func();
    static ae_error_t start_eat_pre_internal();
};
#endif
