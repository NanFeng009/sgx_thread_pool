#ifndef __AESM_THREAD_H__
#define __AESM_THREAD_H__

#include "aeerror.h"
#include <stddef.h>
#include <stdint.h>

#define ETIMEDOUT 110

typedef struct _aesm_thread_t *aesm_thread_t;
typedef ptrdiff_t aesm_thread_arg_type_t;
typedef ae_error_t (*aesm_thread_function_t)(aesm_thread_arg_type_t arg);

ae_error_t aesm_create_thread(aesm_thread_function_t function_entry, aesm_thread_arg_type_t arg, aesm_thread_t* h);
ae_error_t aesm_free_thread(aesm_thread_t h);
ae_error_t aesm_join_thread(aesm_thread_t h, ae_error_t *thread_ret);

extern const uint32_t AESM_THREAD_INFINITE;

ae_error_t aesm_wait_thread(aesm_thread_t h, ae_error_t *thread_ret, unsigned long milisecond);
#endif
