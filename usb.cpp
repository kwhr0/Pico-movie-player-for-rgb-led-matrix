#include "usb.h"
#include "menu.h"
#include "usb.pio.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include <deque>
#include <set>

#define INTERVAL		10	// interrupt transfer period [mS]
#define GPIO_USB		26	// GPIO26 for D-, GPIO27 for D+

static PIO pio;
static uint sm;
static u8 usbbuf[8];

struct NRZI {
	static constexpr int N = 8;
	constexpr NRZI() : b(), d(), ofs(0), len(0), count(0), cur(1) {
		for (u8 i = 0; i < 2 * N; i++) b[i] = 0;
		for (u8 i = 0; i < N; i++) d[i] = 0;
	}
	constexpr void append(u16 c, u8 n) {
		for (u8 i = 0; i < n; i++) {
			if (c >> i & 1) b[ofs >> 3] |= 1 << (ofs & 7);
			ofs++;
		}
	}
	constexpr void sync() { append(0x80, 8); }
	constexpr void ack() { append(0xd2, 8); }
	constexpr void setup() { append(0x2d, 8); }
	constexpr void data0() { append(0xc3, 8); }
	constexpr void in() { append(0x69, 8); }
	constexpr void address(u8 v) { append(v, 7); }
	constexpr void endpoint(u8 v) { append(v, 4); }
	constexpr void bmRequestType(u8 v) { append(v, 8); }
	constexpr void bRequest(u8 v) { append(v, 8); }
	constexpr void wValue(u16 v) { append(v, 16); }
	constexpr void wIndex(u16 v) { append(v, 16); }
	constexpr void wLength(u16 v) { append(v, 16); }
	constexpr u16 calc_crc(u16 poly, u16 mask) {
		u16 data = 0, crc = mask;
		for (u8 i = 16; i < ofs; i++) {
			if (!(i & 7)) data = b[i >> 3];
			bool f = (data ^ crc) & 1;
			data >>= 1;
			crc >>= 1;
			if (f) crc ^= poly;
		}
		return ~crc & mask;
	}
	constexpr void crc5() { append(calc_crc(0x14, 0x1f), 5); }
	constexpr void crc16() { append(calc_crc(0xa001, 0xffff), 16); }
	constexpr void end() {
		auto f = [&](u8 v) {
			len >> 4 < N || printf(""); // compile should fail if overflow
			d[len >> 4] |= v << ((len & 15) << 1);
			len++;
		};
		for (u8 i = 0; i < ofs; i++) {
			if (b[i >> 3] >> (i & 7) & 1) {
				f(cur);
				if (++count < 6) continue;
			}
			f(cur = ~cur & 3);
			count = 0;
		}
		len += 2;
		f(1); f(1);
	}
	u8 ofs, len, count, cur;
	u8 b[2 * N];
	u32 d[N];
};

static constexpr struct EOP : NRZI {
	constexpr EOP() : NRZI() {
		end();
	}
} eop;
static constexpr struct Ack : NRZI {
	constexpr Ack() : NRZI() {
		sync(); ack(); end();
	}
} ack;
static constexpr struct Setup : NRZI {
	constexpr Setup(int a) : NRZI() {
		sync(); setup(); address(a); endpoint(0); crc5(); end();
	}
} setup0(0), setup1(1);
static constexpr struct SetAddress1 : NRZI {
	constexpr SetAddress1() : NRZI() {
		sync(); data0(); bmRequestType(0); bRequest(5); wValue(1); wIndex(0); wLength(0); crc16(); end();
	}
} setaddress1;
static constexpr struct SetConfiguration1 : NRZI {
	constexpr SetConfiguration1() : NRZI() {
		sync(); data0(); bmRequestType(0); bRequest(9); wValue(1); wIndex(0); wLength(0); crc16(); end();
	}
} setconfiguration1;
static constexpr struct DataIn : NRZI {
	constexpr DataIn(int a, int e) : NRZI() {
		sync(); in(); address(a); endpoint(e); crc5(); end();
	}
} datain00(0, 0), datain10(1, 0), datain11(1, 1);

static void __no_inline_not_in_flash_func(usb_trans)(const NRZI &nrzi, bool recv) {
	if (recv) memset(usbbuf, 0, sizeof(usbbuf));
	pio_sm_put_blocking(pio, sm, (recv ? 0x20000 : 0x10000) | nrzi.len - 1);
	for (u8 i = 0; i < nrzi.len + 15 >> 4; i++)
		pio_sm_put_blocking(pio, sm, nrzi.d[i]);
	if (!recv) return;
	u8 d, last = 0, index = 0, count = 0, mask = 1;
	do {
		d = pio_sm_get_blocking(pio, sm);
		if (d == last) {
			if (index < sizeof(usbbuf)) usbbuf[index] |= mask;
			if (!(mask <<= 1)) {
				mask = 1;
				index++;
			}
			count++;
		}
		else {
			last = d;
			if (count < 6 && !(mask <<= 1)) {
				mask = 1;
				index++;
			}
			count = 0;
		}
	} while (d);
}

static bool __no_inline_not_in_flash_func(usb_task)(repeating_timer_t *timer) {
	enum State {
		STATE_INIT, STATE_WAIT, STATE_RECHECK, STATE_RECHECK_DISCON,
		STATE_RESET, STATE_EOP40, STATE_CONFIG, STATE_CONNECTED
	};
	static enum State state, nextstate;
	static u16 count;
	static u8 lastButton;
	auto wait = [&](State s, int c) { nextstate = s; count = c; state = STATE_WAIT; };
	auto DATA0orDATA1 = [] { return usbbuf[1] == 0xc3 || usbbuf[1] == 0x4b; };
	auto datain = [&](const NRZI &pkt) {
		do usb_trans(pkt, true);
		while (!DATA0orDATA1());
		usb_trans(ack, false);
		return false;
	};
	switch (state) {
		case STATE_INIT:
			if (gpio_get(GPIO_USB)) wait(STATE_RECHECK, 500);
			break;
		case STATE_RECHECK:
			if (gpio_get(GPIO_USB)) wait(STATE_RESET, 200);
			else state = STATE_INIT;
			break;
		case STATE_RESET:
			printf("USB: bus reset\n");
			pio_sm_put_blocking(pio, sm, 15000); // 10mS
			wait(STATE_EOP40, 10);
			break;
		case STATE_EOP40:
			usb_trans(eop, false);
			if (++count >= 40) state = STATE_CONFIG;
			break;
		case STATE_CONFIG:
			usb_trans(setup0, false);
			usb_trans(setaddress1, true);
			if (datain(datain00)) break;
			usb_trans(setup1, false);
			usb_trans(setconfiguration1, true);
			if (datain(datain10)) break;
			printf("USB: connected.\n");
			state = STATE_CONNECTED;
			count = 0;
			break;
		case STATE_CONNECTED:
			if (gpio_get(GPIO_USB)) {
				usb_trans(eop, false);
				if (++count < INTERVAL) break;
				count = 0;
				usb_trans(datain11, true);
				if (!DATA0orDATA1()) break;
				usb_trans(ack, false);
#if 0
				printf("PID:%02X B:%02X X:%02X Y:%02X W:%02X\n",
					usbbuf[1], usbbuf[2], usbbuf[3], usbbuf[4], usbbuf[5]);
#endif
				if (usbbuf[5]) menu_wheel(usbbuf[5]);
				if (!(usbbuf[2] & 1) && lastButton & 1) menu_left();
				if (!(usbbuf[2] & 2) && lastButton & 2) menu_right();
				lastButton = usbbuf[2];
			}
			else wait(STATE_RECHECK_DISCON, 500);
			break;
		case STATE_RECHECK_DISCON:
			state = gpio_get(GPIO_USB) ? STATE_CONNECTED : STATE_INIT;
			break;
		case STATE_WAIT:
			if (!--count) state = nextstate;
			break;
	}
	return true;
}

void usb_init(PIO _pio) {
	for (int i = GPIO_USB; i < GPIO_USB + 2; i++) {
		gpio_init(i);
		gpio_set_dir(i, true);
		gpio_pull_down(i);
	}
	pio = _pio;
	uint offset = pio_add_program(pio, &usb_program);
	sm = pio_claim_unused_sm(pio, true);
	usb_program_init(pio, sm, offset);
	static repeating_timer_t timer;
	add_repeating_timer_ms(-1, &usb_task, nullptr, &timer);
}
