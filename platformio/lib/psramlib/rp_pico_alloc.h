#ifndef _RP_PICO_ALLOC_H_
#define _RP_PICO_ALLOC_H_

#include <stdlib.h>

#define RP2350_XIP_CSI_PIN  47

#ifdef __cplusplus
extern "C"
{
#endif
    void *rp_mem_malloc(size_t size);
    void rp_mem_free(void *ptr);
    void *rp_mem_realloc(void *ptr, size_t size);
    void *rp_mem_calloc(size_t num, size_t size);
    size_t rp_mem_max_free_size(void);
#ifdef __cplusplus
}
#endif

#endif
