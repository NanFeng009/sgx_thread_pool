#include <iostream>
#include "mouse.h"
#include "aesm_long_lived_thread.h"
#include "animal_info_logic.h"

#define CHECK_EAT_STATUS \
    if(!query_eat_thread_status()){\
        return 0;\
    }


int main()
{
    CHECK_EAT_STATUS;

    AnimalInfoLogic::start_eat_pre_internal();

    printf("end of main\n");
    return 0;
}
