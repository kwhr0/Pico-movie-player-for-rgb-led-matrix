#include "types.h"
#include "hardware/pio.h"

#ifdef __cplusplus
extern "C" {
#endif

void i2s_init(PIO pio);
void i2s_set_volume(u16 v);
void i2s_prepare();
void i2s_put(s16 *buf, u16 n);
int i2s_get_adjust();

#ifdef __cplusplus
}
#endif
