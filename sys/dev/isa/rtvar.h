/* $OpenBSD: rtvar.h,v 1.1 2002/08/28 21:20:48 mickey Exp $ */

/*
 * Copyright (c) 2001, 2002 Maxim Tsyplakov <tm@oganer.net>,
 *			    Vladimir Popov <jumbo@narod.ru>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* AIMS Lab Radiotrack FM Radio Card device driver */

/*
 * Sanyo LM7000 Direct PLL Frequency Synthesizer
 */

#ifndef _RTVAR_H_
#define _RTVAR_H_

struct rt_softc {
	struct device	sc_dev;

	int		sc_ct;	/* card type */
	int		sc_mute;
	u_int8_t	sc_vol;
	u_int32_t	sc_freq;
	u_int32_t	sc_rf;
	u_int32_t	sc_stereo;

	struct lm700x_t	lm;
};

void	rtattach(struct rt_softc *);

#endif /* _RTVAR_H_ */
