/*	$OpenBSD: radioio.h,v 1.1 2001/10/04 19:17:59 gluk Exp $	*/
/* $RuOBSD: radioio.h,v 1.3 2001/09/29 17:10:16 pva Exp $ */

/*
 * Copyright (c) 2001 Maxim Tsyplakov <tm@oganer.net>,
 *                    Vladimir Popov <jumbo@narod.ru>
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

#ifndef _SYS_RADIOIO_H_
#define _SYS_RADIOIO_H_

#define MIN_FM_FREQ	87500
#define MAX_FM_FREQ	108000

#define IF_FREQ	10700

#define RADIO_CAPS_DETECT_STEREO	(1<<0)
#define RADIO_CAPS_DETECT_SIGNAL	(1<<1)
#define RADIO_CAPS_SET_MONO		(1<<2)
#define RADIO_CAPS_HW_SEARCH		(1<<3)
#define RADIO_CAPS_HW_AFC		(1<<4)
#define RADIO_CAPS_REFERENCE_FREQ	(1<<5)
#define RADIO_CAPS_LOCK_SENSITIVITY	(1<<6)
#define RADIO_CAPS_RESERVED1		(1<<7)
#define RADIO_CAPS_RESERVED2		(0xFF<<8)
#define RADIO_CARD_TYPE			(0xFF<<16)

#define RADIO_INFO_STEREO		(1<<0)
#define RADIO_INFO_SIGNAL		(1<<1)

/* Radio device operations */
#define RIOCSMUTE	_IOW('R', 21, u_long) /* set mute/unmute */
#define RIOCGMUTE	_IOR('R', 21, u_long) /* get mute state */
#define RIOCGVOLU	_IOR('R', 22, u_long) /* get volume */
#define RIOCSVOLU	_IOW('R', 22, u_long) /* set volume */
#define RIOCGMONO	_IOR('R', 23, u_long) /* get mono/stereo */
#define RIOCSMONO	_IOW('R', 23, u_long) /* toggle mono/stereo */
#define RIOCGFREQ	_IOR('R', 24, u_long) /* get frequency (in kHz) */
#define RIOCSFREQ	_IOW('R', 24, u_long) /* set frequency (in kHz) */
#define RIOCSSRCH	_IOW('R', 25, u_long) /* search up/down */
#define RIOCGCAPS	_IOR('R', 26, u_long) /* get card capabilities */
#define RIOCGINFO	_IOR('R', 27, u_long) /* get generic information */
#define RIOCSREFF	_IOW('R', 28, u_long) /* set reference frequency */
#define RIOCGREFF	_IOR('R', 28, u_long) /* get reference frequency */
#define RIOCSLOCK	_IOW('R', 29, u_long) /* set lock sensetivity */
#define RIOCGLOCK	_IOR('R', 29, u_long) /* get lock sensetivity */

#endif /* _SYS_RADIOIO_H_ */
