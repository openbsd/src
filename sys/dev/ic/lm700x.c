/*	$OpenBSD: lm700x.c,v 1.1 2001/10/04 19:46:46 gluk Exp $	*/

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

#include <sys/param.h>
#include <sys/radioio.h>

#include <dev/ic/lm700x.h>

u_long
lm700x_encode_freq(u_long nfreq, u_long rf)
{
	u_char ref_freq;

	switch (rf) {
	case LM700X_REF_100:
		ref_freq = 100;
		break;
	case LM700X_REF_025:
		ref_freq = 25;
		break;
	case LM700X_REF_050:
		/* FALLTHROUGH */
	default:
		ref_freq = 50;
		break;
	}

	nfreq += IF_FREQ;
	nfreq /= ref_freq;

	return nfreq;
}

void
lm700x_hardware_write(struct lm700x_t *lm, u_long data, u_long addon)
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

u_long
lm700x_encode_ref(u_char rf)
{
	u_long ret;

	if (rf < 36)
		ret = LM700X_REF_025;
	else if (rf > 35 && rf < 75)
			ret = LM700X_REF_050;
	else
		ret = LM700X_REF_100;

	return ret;
}

u_char
lm700x_decode_ref(u_long rf)
{
	u_char ret;

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
