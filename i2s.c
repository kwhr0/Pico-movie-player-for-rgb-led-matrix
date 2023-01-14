/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "i2s.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "i2s.pio.h"

#define SNDBUF_N	0x2000

static s32 sndbuf[SNDBUF_N];
static s32 *bufwp;
static uint dma_ch;
static u16 volume;

static void dma_handler() {
	dma_channel_acknowledge_irq0(dma_ch);
	dma_channel_set_read_addr(dma_ch, sndbuf, true); // restart
}

void i2s_init(PIO pio) {
	uint sm = pio_claim_unused_sm(pio, true);
	i2s_program_init(pio, sm);

	dma_ch = dma_claim_unused_channel(true);
	dma_channel_config c = dma_channel_get_default_config(dma_ch);
	channel_config_set_read_increment(&c, true);
	channel_config_set_write_increment(&c, false);
	channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
	channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true)); // TX
	dma_channel_set_config(dma_ch, &c, false); // not start
	dma_channel_set_irq0_enabled(dma_ch, true);
	irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
	irq_set_enabled(DMA_IRQ_0, true);
	dma_channel_set_trans_count(dma_ch, SNDBUF_N, false); // not start
	dma_channel_set_write_addr(dma_ch, &pio->txf[sm], false); // not start
	dma_channel_set_read_addr(dma_ch, sndbuf, true); // start

	pio_sm_set_enabled(pio, sm, true);
}

void i2s_set_volume(u16 v) {
	volume = v;
}

void i2s_prepare() {
	bufwp = &sndbuf[SNDBUF_N / 2 - dma_hw->ch[dma_ch].transfer_count];
	if (bufwp < sndbuf) bufwp += SNDBUF_N;
}

void i2s_put(s16 *buf, u16 n) {
	while (n--) {
		*bufwp = volume * *buf++;
		if (++bufwp >= sndbuf + SNDBUF_N) bufwp = sndbuf;
	}
}

int i2s_get_adjust() {
	s32 *p = &sndbuf[SNDBUF_N - dma_hw->ch[dma_ch].transfer_count];
	int bufn = p <= bufwp ? bufwp - p : SNDBUF_N - (p - bufwp);
//	printf("bufn:%d%%\n", 100 * bufn / SNDBUF_N);
	return (int)bufn - SNDBUF_N / 2;
}
