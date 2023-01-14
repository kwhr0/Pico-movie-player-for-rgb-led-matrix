#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void file_init();
void file_open();
void file_close();
void file_read(void *buf, u32 len);
u32 file_tell();
void file_skip(u32 len);
int file_getN();
int file_get_index();
void file_set_next(int index);
const char *file_get_name();

#ifdef __cplusplus
}
#endif
