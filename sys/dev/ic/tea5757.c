/*	$OpenBSD: tea5757.c,v 1.1 2001/10/04 19:46:46 gluk Exp $	*/

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

/* Implementation of most common TEA5757 routines */

#include <sys/param.h>
#include <sys/radioio.h>

#include <dev/ic/tea5757.h>

/*
 * Convert frequency to hardware representation
 */
u_long
tea5757_encode_freq(u_long freq)
{
#ifdef RADIO_TEA5759
	freq -= IF_FREQ;
#else
	freq += IF_FREQ;
#endif /* RADIO_TEA5759 */
	/*
	 * NO FLOATING POINT!
	 */
	freq *= 10;
	freq /= 125;
	return freq & TEA5757_FREQ;
}

/*
 * Convert frequency from hardware representation
 */
u_long
tea5757_decode_freq(u_long freq)
{
	freq &= TEA5757_FREQ;
	freq *= 125; /* 12.5 kHz */
	freq /= 10;
#ifdef RADIO_TEA5759
	freq += IF_FREQ;
#else
	freq -= IF_FREQ;
#endif /* RADIO_TEA5759 */
	return freq;
}

/*
 * Hardware search
 */
void
tea5757_search(struct tea5757_t *tea, u_long stereo, u_long lock, int dir)
{
	u_long reg;
	u_int co = 0;

	reg = stereo | lock | TEA5757_SEARCH_START;
	reg |= dir ? TEA5757_SEARCH_UP : TEA5757_SEARCH_DOWN;
	tea5757_hardware_write(tea, reg);

	DELAY(TEA5757_ACQUISITION_DELAY);

	do {
		DELAY(TEA5757_WAIT_DELAY);
		reg = tea->read(tea->iot, tea->ioh, tea->offset);
	} while ((reg & TEA5757_FREQ) == 0 && ++co < 200);
}

void
tea5757_hardware_write(struct tea5757_t *tea, u_long data)
{
	int i = TEA5757_REGISTER_LENGTH;

	tea->init(tea->iot, tea->ioh, tea->offset, 0);

	while (i--)
		if (data & (1 << i))
			tea->write_bit(tea->iot, tea->ioh, tea->offset, 1);
		else
			tea->write_bit(tea->iot, tea->ioh, tea->offset, 0);

	tea->rset(tea->iot, tea->ioh, tea->offset, 0);
}

u_long
tea5757_set_freq(struct tea5757_t *tea, u_long stereo, u_long lock, u_long freq)
{
	u_long data = 0ul;

	if (freq < MIN_FM_FREQ)
		freq = MIN_FM_FREQ;
	if (freq > MAX_FM_FREQ)
		freq = MAX_FM_FREQ;

	data = tea5757_encode_freq(freq) | stereo | lock | TEA5757_SEARCH_END;
	tea5757_hardware_write(tea, data);

	return freq;
}

u_long
tea5757_encode_lock(u_char lock)
{
	u_long ret;

	if (lock < 8)
		ret = TEA5757_S005;
	else if (lock > 7 && lock < 15)
		ret = TEA5757_S010;
	else if (lock > 14 && lock < 51)
		ret = TEA5757_S030;
	else if (lock > 50)
		ret = TEA5757_S150;

	return ret;
}

u_char
tea5757_decode_lock(u_long lock)
{
	u_char ret;

	switch (lock) {
	case TEA5757_S005:
		ret = 5;
		break;
	case TEA5757_S010:
		ret = 10;
		break;
	case TEA5757_S030:
		ret = 30;
		break;
	case TEA5757_S150:
		/* FALLTHROUGH */
	default:
		ret = 150;
		break;
	}

	return ret;
}
