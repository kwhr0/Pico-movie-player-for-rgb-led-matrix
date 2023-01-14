#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void menu_init();
void menu_write(u32 *buf);
void menu_left();
void menu_right();
void menu_wheel(s8 d);

#ifdef __cplusplus
}
#endif
