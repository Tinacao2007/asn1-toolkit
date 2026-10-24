#ifndef CGEN_BATCH2_ALLOCATOR_H
#define CGEN_BATCH2_ALLOCATOR_H
#include <stddef.h>
void *batch2_malloc(size_t);
void *batch2_calloc(size_t, size_t);
void batch2_free(void *);
#endif
