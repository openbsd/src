/*	$OpenBSD: mscpreg.h,v 1.5 2003/06/02 23:27:57 millert Exp $	*/
/*	$NetBSD: mscpreg.h,v 1.4 1999/05/29 19:12:53 ragge Exp $	*/
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)udareg.h	7.3 (Berkeley) 5/8/91
 */

/*
 * NRSPL2 and NCMDL2 control the number of response and command
 * packets respectively.  They may be any value from 0 to 7, though
 * setting them higher than 5 is unlikely to be of any value.
 * If you get warnings about your command ring being too small,
 * try increasing the values by one.
 */
#ifndef NRSP
#define NRSPL2	5
#define NCMDL2	5
#define NRSP	(1 << NRSPL2)
#define NCMD	(1 << NCMDL2)
#endif

/*
 * Communication area definition. This seems to be the same for
 * all types of MSCP controllers.
 */

struct mscp_ca {
	short	ca_xxx1;	/* unused */
	char	ca_xxx2;	/* unused */
	char	ca_bdp;		/* BDP to purge */
	short	ca_cmdint;	/* command ring transition flag */
	short	ca_rspint;	/* response ring transition flag */
	long	ca_rspdsc[NRSP];/* response descriptors */
	long	ca_cmddsc[NCMD];/* command descriptors */
};

/*
 * Simplified routines (e.g., uddump) reprogram the UDA50 for one command
 * and one response at a time; uda1ca is like udaca except that it provides
 * exactly one command and response descriptor.
 */
struct mscp_1ca {
	short	ca_xxx1;
	char	ca_xxx2;
	char	ca_bdp;
	short	ca_cmdint;
	short	ca_rspint;
	long	ca_rspdsc;
	long	ca_cmddsc;
};

/*
 * Combined communications area and MSCP packet pools, per controller.
 * NRSP and NCMD must be defined before this struct is used.
 */

struct	mscp_pack {
	struct	mscp_ca mp_ca;		/* communications area */
	struct	mscp mp_rsp[NRSP];	/* response packets */
	struct	mscp mp_cmd[NCMD];	/* command packets */
};

/*
 * Bits in UDA status register during initialisation
 */
#define MP_ERR		0x8000	/* error */
#define MP_STEP4	0x4000	/* step 4 has started */
#define MP_STEP3	0x2000	/* step 3 has started */
#define MP_STEP2	0x1000	/* step 2 has started */
#define MP_STEP1	0x0800	/* step 1 has started */
#define MP_NV		0x0400	/* no host settable interrupt vector */
#define MP_QB		0x0200	/* controller supports Q22 bus */
#define MP_DI		0x0100	/* controller implements diagnostics */
#define MP_IE		0x0080	/* interrupt enable */
#define MP_NCNRMASK	0x003f	/* in STEP1, bits 0-2=NCMDL2, 3-5=NRSPL2 */
#define MP_IVECMASK	0x007f	/* in STEP2, bits 0-6 are interruptvec / 4 */
#define MP_PI		0x0001	/* host requests adapter purge interrupts */
#define MP_GO		0x0001	/* Go command to ctlr */

#define ALLSTEPS	(MP_ERR | MP_STEP4 | MP_STEP3 | MP_STEP2 | MP_STEP1)

#define STEP0MASK	(ALLSTEPS | MP_NV)

#define STEP1MASK	(ALLSTEPS | MP_IE | MP_NCNRMASK) 
#define STEP1GOOD	(MP_STEP2 | MP_IE | (NCMDL2 << 3) | NRSPL2)
 
#define STEP2MASK	(ALLSTEPS | MP_IE | MP_IVECMASK)
#define STEP2GOOD(iv)	(MP_STEP3 | MP_IE | (iv))
 
#define STEP3MASK	ALLSTEPS
#define STEP3GOOD	MP_STEP4

