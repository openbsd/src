/*	$OpenBSD: tc921x.c,v 1.3 2007/05/22 04:14:03 jsg Exp $	*/

/*
 * Copyright (c) 2001, 2002 Vladimir Popov <jumbo@narod.ru>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Toshiba's High Speed PLL for DTS
 *
 * TC9216P, TC9217P, TC9217F are a high speed PLL-LSI with built-in 2 modulus
 * prescaler. Each function is controlled through 3 serial bus lines and high
 * performance digital tuning system can be constituted.
 *
 * Each function is controlled by the data setting to a pair of 24-bit
 * registers. Each data of these registers is exchanged with controller side
 * by 3 serial lines of DATA, CLOCK and PERIOD.
 *
 * 8 address bits and 24 data bits, total 32 bits, are transferred thru
 * serial port.
 *
 * Input data is latched to the first and second input registers at the fall
 * of PERIOD signal and each function is activated.
 *
 * Each output data is latched to output register in parallel at the fall
 * timing of the 9th of CLOCK signal and can be received serially over the
 * DATA line. Serial data of DATA, CLOCK and PERIOD is synchronized with
 * crystal oscillation clock and tacken into the internal circuit of LSI.
 * Thus, if crystal oscillator is stopped, serial data can not be input.
 */

#include <sys/param.h>
#include <sys/radioio.h>

#include <dev/ic/tc921x.h>

#define PL_CL_DL(c)	((0 << c->period) | (0 << c->clock) | (0 << c->data))
#define PL_CL_DH(c)	((0 << c->period) | (0 << c->clock) | (1 << c->data))
#define PL_CH_DL(c)	((0 << c->period) | (1 << c->clock) | (0 << c->data))
#define PL_CH_DH(c)	((0 << c->period) | (1 << c->clock) | (1 << c->data))

#define PH_CL_DL(c)	((1 << c->period) | (0 << c->clock) | (0 << c->data))
#define PH_CL_DH(c)	((1 << c->period) | (0 << c->clock) | (1 << c->data))
#define PH_CH_DL(c)	((1 << c->period) | (1 << c->clock) | (0 << c->data))
#define PH_CH_DH(c)	((1 << c->period) | (1 << c->clock) | (1 << c->data))

#define PERIOD_LOW	0
#define PERIOD_HIGH	1

static void __tc921x_write_burst(unsigned int, u_int32_t, struct tc921x_t *, int);
static u_int32_t __tc921x_read_burst(unsigned int, struct tc921x_t *);

u_int32_t
tc921x_encode_freq(u_int32_t freq) {
	/* Normalize incoming frequency */
	if (freq < MIN_FM_FREQ)
		freq = MIN_FM_FREQ;
	if (freq > MAX_FM_FREQ)
		freq = MAX_FM_FREQ;

	return (freq + IF_FREQ)/10;
}

u_int32_t
tc921x_decode_freq(u_int32_t reg) {
	return (reg & TC921X_D0_FREQ_DIVIDER) * 10 - IF_FREQ;
}

u_int32_t
tc921x_read_addr(struct tc921x_t *c, u_int8_t addr) {
	u_int32_t ret;

	/* Finish previous transmission - PERIOD HIGH, CLOCK HIGH, DATA HIGH */
	bus_space_write_1(c->iot, c->ioh, c->offset, PH_CH_DH(c));
	/* Start transmission - PERIOD LOW, CLOCK HIGH, DATA HIGH */
	bus_space_write_1(c->iot, c->ioh, c->offset, PL_CH_DH(c));

	/*
	 * Period must be low when the register address transmission starts.
	 * Period must be high when the register data transmission starts.
	 * Do the switch in the middle of the address transmission.
	 */
	__tc921x_write_burst(4, addr, c, PERIOD_LOW);
	__tc921x_write_burst(4, addr >> 4, c, PERIOD_HIGH);

	/* Reading data from the register */
	ret = __tc921x_read_burst(TC921X_REGISTER_LENGTH, c);

	/* End of transmission - PERIOD goes LOW then HIGH */
	bus_space_write_1(c->iot, c->ioh, c->offset, PL_CH_DH(c));
	bus_space_write_1(c->iot, c->ioh, c->offset, PH_CH_DH(c));

	return ret;
}

void
tc921x_write_addr(struct tc921x_t *c, u_int8_t addr, u_int32_t reg) {
	/* Finish previous transmission - PERIOD HIGH, CLOCK HIGH, DATA HIGH */
	bus_space_write_1(c->iot, c->ioh, c->offset, PH_CH_DH(c));
	/* Start transmission - PERIOD LOW, CLOCK HIGH, DATA HIGH */
	bus_space_write_1(c->iot, c->ioh, c->offset, PL_CH_DH(c));

	/*
	 * Period must be low when the register address transmission starts.
	 * Period must be high when the register data transmission starts.
	 * Do the switch in the middle of the address transmission.
	 */
	__tc921x_write_burst(4, addr, c, PERIOD_LOW);
	__tc921x_write_burst(4, addr >> 4, c, PERIOD_HIGH);

	/* Writing data to the register */
	__tc921x_write_burst(TC921X_REGISTER_LENGTH, reg, c, 1);

	/* End of transmission - PERIOD goes LOW then HIGH */
	bus_space_write_1(c->iot, c->ioh, c->offset, PL_CH_DH(c));
	bus_space_write_1(c->iot, c->ioh, c->offset, PH_CH_DH(c));
}

static void
__tc921x_write_burst(unsigned int length, u_int32_t data, struct tc921x_t *c, int p) {
	int i;
	u_int8_t cldh, chdh, cldl, chdl;

	cldh = p == PERIOD_LOW ? PL_CL_DH(c) : PH_CL_DH(c);
	chdh = p == PERIOD_LOW ? PL_CH_DH(c) : PH_CH_DH(c);
	cldl = p == PERIOD_LOW ? PL_CL_DL(c) : PH_CL_DL(c);
	chdl = p == PERIOD_LOW ? PL_CH_DL(c) : PH_CH_DL(c);

	for (i = 0; i < length; i++)
		if (data & (1 << i)) {
			bus_space_write_1(c->iot, c->ioh, c->offset, cldh);
			bus_space_write_1(c->iot, c->ioh, c->offset, chdh);
		} else {
			bus_space_write_1(c->iot, c->ioh, c->offset, cldl);
			bus_space_write_1(c->iot, c->ioh, c->offset, chdl);
		}
}

static u_int32_t
__tc921x_read_burst(unsigned int length, struct tc921x_t *c) {
	unsigned int i;
	u_int32_t ret = 0ul;

#define DATA_ON	(1 << c->data)

	for (i = 0; i < length; i++) {
		bus_space_write_1(c->iot, c->ioh, c->offset, PH_CL_DH(c));
		bus_space_write_1(c->iot, c->ioh, c->offset, PH_CH_DH(c));
		ret |= bus_space_read_1(c->iot, c->ioh, c->offset) & DATA_ON ?
			(1 << i) : (0 << i);
	}

	return ret;
}
