/*	$OpenBSD: fmsradio.h,v 1.1 2002/05/06 16:37:43 mickey Exp $	*/

/*
 * Copyright (c) 2002 Vladimir Popov <jumbo@narod.ru>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Device Driver for FM801-based FM Tuners */

/* Currently supported tuners:
 *  o MediaForte SoundForte SF64-PCR PCI Radio Card
 *  o MediaForte SoundForte SF256-PCP-R PCI Sound Card with FM tuner
 */

#ifndef _FMS_RADIO_H_
#define _FMS_RADIO_H_

#include <dev/ic/tea5757.h>

struct fmsradio_if {
	int			card; /* Card type */

	int			mute;
	u_int8_t		vol;
	u_int32_t		freq;
	u_int32_t		stereo;
	u_int32_t		lock;

	struct tea5757_t	tea;
};

int	fmsradio_attach(struct fmsradio_if *, char *);

int	fmsradio_get_info(void *, struct radio_info *);
int	fmsradio_set_info(void *, struct radio_info *);
int	fmsradio_search(void *, int);

#endif /* _FMS_RADIO_H_ */
