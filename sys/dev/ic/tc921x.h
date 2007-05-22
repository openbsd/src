/*	$OpenBSD: tc921x.h,v 1.3 2007/05/22 04:14:03 jsg Exp $	*/

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
 */

#ifndef _TC921X_H_
#define _TC921X_H_

#include <sys/types.h>

#include <machine/bus.h>

#define TC921X_REGISTER_LENGTH	24

/* Input Register at 0xD0 */
#define TC921X_D0_FREQ_DIVIDER			0xFFFF

/* (*) are only available at 4.5 MHz crystal resonator used */
#define TC921X_D0_REF_FREQ_500_HZ		(0x0 << 16)
#define TC921X_D0_REF_FREQ_1_KHZ		(0x1 << 16)
#define TC921X_D0_REF_FREQ_2P5_KHZ		(0x2 << 16)
#define TC921X_D0_REF_FREQ_3_KHZ		(0x3 << 16)
#define TC921X_D0_REF_FREQ_3P125_KHZ		(0x4 << 16)
#define TC921X_D0_REF_FREQ_3PXXX_KHZ		(0x5 << 16) /* (*) */
#define TC921X_D0_REF_FREQ_5_KHZ		(0x6 << 16)
#define TC921X_D0_REF_FREQ_6P25_KHZ		(0x7 << 16)
#define TC921X_D0_REF_FREQ_7PXXX_KHZ		(0x8 << 16) /* (*) */
#define TC921X_D0_REF_FREQ_9_KHZ		(0x9 << 16)
#define TC921X_D0_REF_FREQ_10_KHZ		(0xA << 16)
#define TC921X_D0_REF_FREQ_12P5_KHZ		(0xB << 16)
#define TC921X_D0_REF_FREQ_25_KHZ		(0xC << 16)
#define TC921X_D0_REF_FREQ_50_KHZ		(0xD << 16)
#define TC921X_D0_REF_FREQ_100_KHZ		(0xE << 16)
#define TC921X_D0_REF_FREQ_NOT_USED		(0xF << 16)

#define TC921X_D0_DIRECT_DIVIDING_MODE		(0 << 20)
#define TC921X_D0_PULSE_SWALLOW_HF_MODE		(2 << 20)
#define TC921X_D0_PULSE_SWALLOW_FM_MODE		(1 << 20)
#define TC921X_D0_HALF_PULSE_SWALLOW_MODE	(3 << 20)

#define TC921X_D0_OSC_7POINT2_MHZ		(1 << 22)
#define TC921X_D0_OSC_4POINT5_MHZ		(0 << 22)

#define TC921X_D0_OUT_CONTROL_ON		(1 << 23)
#define TC921X_D0_OUT_CONTROL_OFF		(0 << 23)

/* Input Register at 0xD2 */
#define TC921X_D2_GATE_TIME(x)			(x << 0)
#define		TC921X_D2_GATE_TIME_1MS		TC921X_D2_GATE_TIME(0)
#define		TC921X_D2_GATE_TIME_4MS		TC921X_D2_GATE_TIME(1)
#define		TC921X_D2_GATE_TIME_16MS	TC921X_D2_GATE_TIME(2)
#define		TC921X_D2_GATE_TIME_MANUAL	TC921X_D2_GATE_TIME(3)

#define TC921X_D2_COUNTER_MODE(x)		(x << 2)

#define TC921X_D2_COUNTER_INPUT_SC		(1 << 5)
#define TC921X_D2_COUNTER_INPUT_HFC		(1 << 6)
#define TC921X_D2_COUNTER_INPUT_LFC		(1 << 7)

#define TC921X_D2_START_BIT			(1 << 8)
#define TC921X_D2_TEST_BIT			(1 << 9)

#define TC921X_D2_IO_PORT(x)			(x << 10)
#define TC921X_D2_IO_PORT_OUTPUT(x)		(x << 15)
#define TC921X_D2_IO_PORT_INPUT(x)		(x << 19)

struct tc921x_t {
	bus_space_tag_t	iot;
	bus_space_handle_t	ioh;
	bus_size_t	offset;

	u_int8_t	period;
	u_int8_t	clock;
	u_int8_t	data;
};

void	tc921x_write_addr(struct tc921x_t *, u_int8_t, u_int32_t);
u_int32_t	tc921x_read_addr(struct tc921x_t *, u_int8_t);
u_int32_t	tc921x_encode_freq(u_int32_t);
u_int32_t	tc921x_decode_freq(u_int32_t);

#endif /* _TC921X_H_ */
