/*	$OpenBSD: opti.h,v 1.6 2003/06/02 19:24:22 mickey Exp $	*/

/*
 * Copyright (c) 1996 Michael Shalayeff
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef	__OPTI_HEADER__
#define	__OPTI_HEADER__

	/* card types */

#define OPTI_C928	0xe2
#define OPTI_MOZART	0xe2
#define OPTI_C929	0xe3
#define OPTI_C930	0xe3	/* XXX 0xe4 ??? */

	/* i/o ports */

#define	OPTI_4			0xF84
#define	OPTI_5			0xF85
#define	OPTI_DATA		0xF8E
#define	OPTI_IFTP		0xF8D	/* iface control register */
#define	OPTI_PASSWD		0xF8F
#define	OPTI_ENBL		0xF91

#define	OPTI_SND_MASK	0xf1

	/* CD-ROM iface setup */

		/* CD-ROM interface types */
#define OPTI_DISABLE		0
#define	OPTI_SONY		1
#define	OPTI_MITSUMI		2
#define	OPTI_PANASONIC		3
#define	OPTI_IDE		4

	/* Sound system setup */

		/* Sound iface types */
#define	OPTI_WSS	(0)	/* Windows Sound System */
#define OPTI_SB		(1)	/* Sound Blaster Pro(tm) compatible */

#ifdef _KERNEL
int	opti_cd_setup( int, int, int, int );
int	opti_snd_setup( int, int, int, int );
#endif

#endif	/* __OPTI_HEADER__ */
