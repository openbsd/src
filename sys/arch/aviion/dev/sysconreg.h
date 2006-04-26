/*	$OpenBSD: sysconreg.h,v 1.1.1.1 2006/04/26 14:24:54 miod Exp $ */

/*
 * Copyright (c) 2004, Miodrag Vallat.
 * Copyright (c) 1999 Steve Murphree, Jr.
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
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * SYSCON interrupt vectors (reserved for non-VME interrupt sources)
 */
#define	SYSCON_VECT	0x50
#define	SYSCON_NVEC	0x10

#define SYSCV_ABRT	0x02	/* abort button */
#define SYSCV_SYSF	0x03	/* SYSFAIL */
#define SYSCV_ACF 	0x04	/* ACFAIL */
#define SYSCV_SCC 	0x05	/* dart(4) serial interrupts */
#define SYSCV_TIMER2    0x06	/* profiling clock */
#define	SYSCV_SCC2	0x08	/* second dart(4) instance */
#define	SYSCV_LE	0x09	/* onboard ethernet */
#define	SYSCV_SCSI	0x0a	/* onboard SCSI */

int	sysconintr_establish(u_int, struct intrhand *, const char *);
