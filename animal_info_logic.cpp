#include "animal_info_logic.h"

#include <stdio.h>
#include "aesm_long_lived_thread.h"
#include <unistd.h>

/* File to declare AnimalInfoLogic Class which is animal independent */

ae_error_t AnimalInfoLogic::start_eat_pre_internal()  //entrace function
{
    printf("enter AnimalInfoLogic::start_eat_pre_internal\n");

    //start_eat_thread( 5000); // 5 seconds
    start_eat_thread(AESM_THREAD_INFINITE);

    printf("leave AnimalInfoLogic::start_eat_pre_internal\n");

    return AE_SUCCESS;
}

ae_error_t AnimalInfoLogic::check_eat_thread_func() //exit function
{
    printf("enter AnimalInfoLogic::check_eat_thread_func\n");
    printf("start sleep 6 seconds         ------------------------------\n");
    sleep(7);
    printf("start wake after 6 seconds         ------------------------------\n");

    printf("leave AnimalInfoLogic::check_eat_thread_func\n");

    return AE_SUCCESS;
}


