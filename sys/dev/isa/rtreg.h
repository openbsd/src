/* $OpenBSD: rtreg.h,v 1.1 2002/08/28 21:20:48 mickey Exp $ */

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

#ifndef _RTREG_H_
#define _RTREG_H_

#include <dev/isa/rtvar.h>

#define RF_25K			25
#define RF_50K			50
#define RF_100K			100

#define MAX_VOL			5	/* XXX Find real value */
#define VOLUME_RATIO(x)		(255 * x / MAX_VOL)

#define RT_BASE_VALID(x)	((x == 0x20C) || (x == 0x30C))

#define CARD_RADIOTRACK		0
#define CARD_SF16FMI		1
#define CARD_UNKNOWN		-1

#define RTRACK_CAPABILITIES	RADIO_CAPS_DETECT_STEREO | 		\
				RADIO_CAPS_DETECT_SIGNAL | 		\
				RADIO_CAPS_SET_MONO | 			\
				RADIO_CAPS_REFERENCE_FREQ

#define SF16FMI_CAPABILITIES	RADIO_CAPS_REFERENCE_FREQ

#define	RT_WREN_ON		(1 << 0)
#define	RT_WREN_OFF		(0 << 0)
#define RT_CLCK_ON		(1 << 1)
#define RT_CLCK_OFF		(0 << 1)
#define RT_DATA_ON		(1 << 2)
#define RT_DATA_OFF		(0 << 2)
#define RT_CARD_ON		(1 << 3)
#define RT_CARD_OFF		(0 << 3)
#define RT_SIGNAL_METER		(1 << 4)
#define RT_VOLUME_DOWN		(1 << 6)
#define RT_VOLUME_UP		(2 << 6)
#define RT_VOLUME_STEADY	(3 << 6)

#define RT_SIGNAL_METER_DELAY	150000
#define RT_VOLUME_DELAY		100000

#endif /* _RTREG_H_ */
