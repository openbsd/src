/*	$OpenBSD: lm700x.c,v 1.3 2007/05/22 04:14:03 jsg Exp $	*/

/*
 * Copyright (c) 2001 Vladimir Popov <jumbo@narod.ru>
 * All rights reserved.
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

/* Implementation of most common lm700x routines */

/*
 * Sanyo LM7001 Direct PLL Frequency Synthesizer
 *
 * The LM7001J and LM7001JM (used in Aztech/PackardBell cards) are PLL
 * frequency synthesizer LSIs for tuners. These LSIs are software compatible
 * with LM7000 (used in Radiotrack, Radioreveal RA300, some Mediaforte cards),
 * but do not include an IF calculation circuit.
 *
 * The FM VCO circuit includes a high-speed programmable divider that can
 * divide directly.
 *
 * Features:
 * Seven reference frequencies: 1, 5, 9, 10, 25, 50, and 100 kHz;
 * Band-switching outputs (3 bits);
 * Controller clock output (400 kHz);
 * Serial input circuit for data input (using the CE, CL and DATA pins).
 *
 * The LM7001J and LM7001JM have a 24-bit shift register.
 */

#include <sys/param.h>
#include <sys/radioio.h>

#include <dev/ic/lm700x.h>

u_int32_t
lm700x_encode_freq(u_int32_t nfreq, u_int32_t rf)
{
	nfreq += IF_FREQ;
	nfreq /= lm700x_decode_ref(rf);
	return nfreq;
}

void
lm700x_hardware_write(struct lm700x_t *lm, u_int32_t data, u_int32_t addon)
{
	int i;

	lm->init(lm->iot, lm->ioh, lm->offset, lm->rsetdata | addon);

	for (i = 0; i < LM700X_REGISTER_LENGTH; i++)
		if (data & (1 << i)) {
			bus_space_write_1(lm->iot, lm->ioh, lm->offset,
					lm->wocl | addon);
			DELAY(LM700X_WRITE_DELAY);
			bus_space_write_1(lm->iot, lm->ioh, lm->offset,
					lm->woch | addon);
			DELAY(LM700X_WRITE_DELAY);
			bus_space_write_1(lm->iot, lm->ioh, lm->offset,
					lm->wocl | addon);
		} else {
			bus_space_write_1(lm->iot, lm->ioh, lm->offset,
					lm->wzcl | addon);
			DELAY(LM700X_WRITE_DELAY);
			bus_space_write_1(lm->iot, lm->ioh, lm->offset,
					lm->wzch | addon);
			DELAY(LM700X_WRITE_DELAY);
			bus_space_write_1(lm->iot, lm->ioh, lm->offset,
					lm->wzcl | addon);
		}

	lm->rset(lm->iot, lm->ioh, lm->offset, lm->rsetdata | addon);
}

u_int32_t
lm700x_encode_ref(u_int8_t rf)
{
	u_int32_t ret;

	if (rf < 36)
		ret = LM700X_REF_025;
	else if (rf > 35 && rf < 75)
			ret = LM700X_REF_050;
	else
		ret = LM700X_REF_100;

	return ret;
}

u_int8_t
lm700x_decode_ref(u_int32_t rf)
{
	u_int8_t ret;

	switch (rf) {
	case LM700X_REF_100:
		ret = 100;
		break;
	case LM700X_REF_025:
		ret = 25;
		break;
	case LM700X_REF_050:
		/* FALLTHROUGH */
	default:
		ret = 50;
		break;
	}

	return ret;
}
