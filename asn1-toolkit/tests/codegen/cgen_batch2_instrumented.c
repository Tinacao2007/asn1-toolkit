/* Only this test copy of generated C sees allocation fault injection. */
#include <stdlib.h>
#include "cgen_batch2_allocator.h"
#define malloc batch2_malloc
#define calloc batch2_calloc
#define free batch2_free
#include "generated.c"
