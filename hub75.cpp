/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hub75.h"
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hub75.pio.h"

#define FPS		30

static u32 rgb[2][PWMSEG][HEIGHT / 2][WIDTH];
static u64 next;
static int gamma_index, gamma_index_next;
static int pwmbits, pwmbits_next;
static int rps, rps_next;
static uint sm_rgb, sm_row, dma_ch;
static PIO pio;
volatile static int sw;

static constexpr struct Gamma {
	constexpr Gamma() : tbl() {
		for (int i = 0; i < GAMMA_N; i++) {
			float g = gamma_value(i), b = powf(255.f, g);
			for (int j = 0; j < 256; j++) {
				uint s = ((1 << PWMMAX) - 1) * powf(j, g) / b + .5f;
				for (int k = PWMSEG - 1; k >= 0; k--) {
					u32 d = 0;
					for (int l = 4; l >= 0; l--) {
						d <<= 6;
						s <<= 1;
						if (s & 1 << PWMMAX) d |= 1;
					}
					tbl[i][j][k] = d;
				}
			}
		}
	}
	void conv(u32 *buf) const {
		const u32 (*t)[PWMSEG] = tbl[gamma_index];
		u32 *sp0 = buf, *sp1 = &buf[WIDTH * HEIGHT / 2];
		u32 *dp[PWMSEG];
		for (int i = 0; i < PWMSEG; i++) dp[i] = rgb[!sw][i][0];
		for (int y = 0; y < HEIGHT / 2; y++)
			for (int x = 0; x < WIDTH; x++) {
				u32 s0 = *sp0++, s1 = *sp1++;
				const u32 *g00 = t[s0 & 0xff], *g01 = t[s0 >> 16 & 0xff], *g02 = t[s0 >> 8 & 0xff];
				const u32 *g10 = t[s1 & 0xff], *g11 = t[s1 >> 16 & 0xff], *g12 = t[s1 >> 8 & 0xff];
				for (int i = 0; i < PWMSEG; i++)
					*dp[i]++ = g02[i] << 2 | g01[i] << 1 | g00[i] |
						g12[i] << 5 | g11[i] << 4 | g10[i] << 3;
			}
	}
	u32 tbl[GAMMA_N][256][PWMSEG];
} gammaTable;

static void wait_tx_stall(PIO pio, uint sm) {
	u32 txstall_mask = 1u << (PIO_FDEBUG_TXSTALL_LSB + sm);
	pio->fdebug = txstall_mask;
	while (!(pio->fdebug & txstall_mask))
		tight_loop_contents();
}

void hub75_init(PIO _pio) {
	pio = _pio;
	sm_rgb = pio_claim_unused_sm(pio, true);
	hub75_rgb_program_init(pio, sm_rgb);
	sm_row = pio_claim_unused_sm(pio, true);
	hub75_row_program_init(pio, sm_row);

	dma_ch = dma_claim_unused_channel(true);
	dma_channel_config c = dma_channel_get_default_config(dma_ch);
	channel_config_set_read_increment(&c, true);
	channel_config_set_write_increment(&c, false);
	channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
	channel_config_set_dreq(&c, pio_get_dreq(pio, sm_rgb, true)); // TX
	dma_channel_set_config(dma_ch, &c, false); // not start
	dma_channel_set_write_addr(dma_ch, &pio->txf[sm_rgb], false); // not start
}

void hub75_main() {
	next = 0;
	while (1) {
		gamma_index = gamma_index_next;
		pwmbits = pwmbits_next;
		rps = rps_next;
		uint loop = (u64)clock_get_hz(clk_sys) * (1 << pwmbits) / (rps * (HEIGHT / 2) * ((1 << pwmbits) - 1));
		for (int y = 0; y < HEIGHT / 2; y++)
			for (int bit = 0; bit < pwmbits; bit++) {
				uint n = (loop >> pwmbits - 1 - bit) + 1 >> 1;
				if (!n) continue;
				uint b = bit + PWMMAX - pwmbits;
				hub75_rgb_set_shift(pio, sm_rgb, 6 * (b % 5));
				dma_channel_transfer_from_buffer_now(dma_ch, rgb[sw][b / 5][y], WIDTH);
				dma_channel_wait_for_finish_blocking(dma_ch);
				wait_tx_stall(pio, sm_rgb);
				wait_tx_stall(pio, sm_row);
				pio_sm_put_blocking(pio, sm_row, y | n - 1 << 5);
			}
		if (to_us_since_boot(get_absolute_time()) >= next) {
			next += 1000000 / FPS;
			sw = !sw;
		}
	}
}

void hub75_copy_and_wait(u32 *buf, int time) {
	gammaTable.conv(buf);
	static int lastsw;
	while (lastsw == sw)
		tight_loop_contents();
	lastsw = sw;
	next += time >> 4;
}

void hub75_cls() {
	memset(rgb[!sw], 0, sizeof(rgb[0]));
	for (int t = sw; t == sw;)
		tight_loop_contents();
	memset(rgb[!sw], 0, sizeof(rgb[0]));
}

void hub75_set_gamma_index(int v) {
	gamma_index_next = v;
}

void hub75_set_pwmbits(int v) {
	pwmbits_next = v;
}

void hub75_set_rps(int v) {
	rps_next = v;
}
