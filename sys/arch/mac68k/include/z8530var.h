/*	$OpenBSD: z8530var.h,v 1.3 1996/06/08 16:21:15 briggs Exp $	*/
/*	$NetBSD: z8530var.h,v 1.2 1996/06/07 10:27:19 briggs Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)zsvar.h	8.1 (Berkeley) 6/11/93
 */

#include <arch/mac68k/dev/z8530sc.h>
#include <arch/mac68k/dev/z8530tty.h>

/*
 * Functions to read and write individual registers in a channel.
 * The ZS chip requires a 1.6 uSec. recovery time between accesses,
 * and the Sun3 hardware does NOT take care of this for you.
 * MacII hardware DOES dake care of the delay for us. :-)
 */

u_char zs_read_reg __P((struct zs_chanstate *cs, u_char reg));
u_char zs_read_csr __P((struct zs_chanstate *cs));
u_char zs_read_data __P((struct zs_chanstate *cs));

void  zs_write_reg __P((struct zs_chanstate *cs, u_char reg, u_char val));
void  zs_write_csr __P((struct zs_chanstate *cs, u_char val));
void  zs_write_data __P((struct zs_chanstate *cs, u_char val));

/*
 * abort detection on console will now timeout after iterating on a loop
 * the following # of times. Cheep hack. Also, abort detection is turned
 * off after a timeout (i.e. maybe there's not a terminal hooked up).
 */
#define ZSABORT_DELAY 3000000

/*
 * How to request a "soft" interrupt.
 * This could be a macro if you like.
 */
void zsc_req_softint __P((struct zsc_softc *zsc));

/* Handle user request to enter kernel debugger. */
void	zs_abort __P((struct zstty_softc *zst));

/* Hook for MD ioctl support */
int	zsmdioctl __P((struct tty *tp, u_long com, caddr_t data, int flag,
	    struct proc *p));

/* Clean up at end of tty attach */
void zstty_mdattach __P((struct zsc_softc *zsc, struct zstty_softc *zst,
	    struct zs_chanstate *cs, struct tty *tp));  

/* Callback for "external" clock sources */
void zsmd_setclock  __P((struct zs_chanstate *cs));

/*
 * Some warts needed by z8530tty.c -
 */
#define	ZSTTY_MAJOR 	12		/* XXX */
#undef	ZSTTY_DEF_CFLAG
#define	ZSTTY_DEF_CFLAG 	(CREAD | CS8 | HUPCL)

#define	ZSTTY_RAW_CFLAG (CS8 | CREAD | HUPCL )
#define	ZSTTY_RAW_IFLAG (IXANY | IMAXBEL)
#define	ZSTTY_RAW_LFLAG (ECHOE|ECHOKE|ECHOCTL)
#define	ZSTTY_RAW_OFLAG (ONLCR | OXTABS)
/* Above taken from looking at a tty after a stty raw */

/* Booter flags interface */
#define ZSMAC_RAW	0x01
#define ZSMAC_LOCALTALK	0x02

#define zsprintf printf
