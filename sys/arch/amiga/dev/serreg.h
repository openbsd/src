/*	$NetBSD: serreg.h,v 1.7 1994/10/26 02:04:54 cgd Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1990 Regents of the University of California.
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
 *	@(#)dcareg.h	7.3 (Berkeley) 5/7/91
 */

struct serdevice {
	int	dummy;
};

/*
 * WARNING: Serial console is assumed to be at SC9
 * and CONUNIT must be 0.
 */
#define CONUNIT		(0)

/* seems this is nowhere defined in the system headers.. do it here */
#define SERDATRF_OVRUN	(1<<15)
#define SERDATRF_RBF	(1<<14)
#define SERDATRF_TBE	(1<<13)
#define SERDATRF_TSRE	(1<<12)
#define SERDATRF_RXD	(1<<11)
#define SERDATRF_RSVD	(1<<10)
#define SERDATRF_STP2	(1<<9)
#define SERDATRF_STP1	(1<<8)

#define ADKCONF_SETCLR	(1<<15)
#define ADKCONF_UARTBRK	(1<<11)


#define SERBRD(val)	((3579545/val-1) < 32768 ? 3579545/val-1 : 0)
#define SER_VBL_PRIORITY (1)

/* unit is in lower 7 bits (for now, only one unit:-))
   dialin:    open blocks until carrier present, hangup on carrier drop
   dialout:   carrier is ignored */

#define SERUNIT(dev)   (minor(dev) & 0x7f)
#define DIALOUT(dev)   ((minor(dev) & 0x80) == 0x00)
