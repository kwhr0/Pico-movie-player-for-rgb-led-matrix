#include "types.h"
#include "hardware/pio.h"

#ifdef __cplusplus
extern "C" {
#endif

void usb_init(PIO);
bool keyget(u8 *, u8 *);

#ifdef __cplusplus
}
#endif
