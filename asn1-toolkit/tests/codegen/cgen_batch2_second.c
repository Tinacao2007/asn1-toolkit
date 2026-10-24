#include "generated.h"
int batch2_second(void) {
  stage2_mA_tFlag value;
  stage2_mA_tFlag_init(&value);
  value.value = 1;
  stage2_mA_tFlag_free(&value);
  return value.value;
}
