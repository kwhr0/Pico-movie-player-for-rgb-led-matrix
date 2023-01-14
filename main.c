#include "file.h"
#include "hub75.h"
#include "i2s.h"
#include "usb.h"
#include "menu.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"

#define CLK_MHZ		128

static u32 buf[WIDTH * HEIGHT];

static inline u32 get4() {
	u32 r;
	file_read(&r, 4);
	return r;
}

static u32 avi_prepare() {
	if (get4() != 0x46464952/*RIFF*/) return 0;
	get4();
	if (get4() != 0x20495641/*AVI */) return 0;
	while (1) {
		u32 type = get4(), len = get4();
		if (type == 0x5453494c/*LIST*/) {
			len -= 4;
			if (get4() == 0x69766f6d/*movi*/) return file_tell() + len;
		}
		file_skip(len);
	}
}

static void play() {
	file_open();
	i2s_prepare();
	for (u32 end = avi_prepare(); file_tell() < end;) {
		u32 type = get4(), len = get4();
		switch (type >> 16 & 0xff) {
		case 'd':
			if (!len) break;
			if (len != sizeof(buf)) panic("video format error\n");
			file_read(buf, len);
			menu_write(buf);
			hub75_copy_and_wait(buf, i2s_get_adjust());
			break;
		case 'w':
			if (len > sizeof(buf)) panic("audio format error\n");
			file_read(buf, len);
			i2s_put((s16 *)buf, len / sizeof(s16));
			break;
		default:
			file_skip(len);
			break;
		}
	}
	hub75_cls();
	file_close();
}

int main() {
	set_sys_clock_khz(1000 * CLK_MHZ, true);
	// set_sys_clock_khz()を呼ぶと、clk_periが48MHzに設定されるので、clk_sysに設定し直す
	clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, CLK_MHZ * MHZ, CLK_MHZ * MHZ);
	stdio_init_all();
	file_init();
	menu_init();
	usb_init(pio0);
	i2s_init(pio0);
	hub75_init(pio0);
	multicore_launch_core1(hub75_main);
	while (1) play();
}
