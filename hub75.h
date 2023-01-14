#include "types.h"
#include "hardware/pio.h"

#define WIDTH		128
#define HEIGHT		64
#define PWMSEG		3
#define PWMMAX		(5 * PWMSEG)
#define GAMMA_N		21
#define GAMMA_STD	12

#define gamma_value(index)	(.1f * (index) + 1.f)

#ifdef __cplusplus
extern "C" {
#endif

void hub75_init(PIO pio);
void hub75_main();
void hub75_cls();
void hub75_copy_and_wait(u32 *buf, int time);
void hub75_set_gamma_index(int gamma);
void hub75_set_pwmbits(int bits);
void hub75_set_rps(int rps);

#ifdef __cplusplus
}
#endif
