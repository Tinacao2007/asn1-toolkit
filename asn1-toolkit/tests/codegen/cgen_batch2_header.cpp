// Standalone C++17 consumer of the same public C header.
#include "generated.h"
static_assert(sizeof(stage2_mA_tRecord) > 0, "complete C layout");
void (*batch2_c_linkage)(stage2_mA_tRecord*) = &stage2_mA_tRecord_free;
