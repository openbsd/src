/*	$OpenBSD: clock.h,v 1.9 2011/09/15 00:48:24 miod Exp $ */
/*	$NetBSD: clock.h,v 1.4 1999/09/06 19:52:53 ragge Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
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
 *	This product includes software developed at Ludd, University of 
 *	Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Time constants. These are unlikely to change.
 */
#define IS_LEAPYEAR(y) (((y % 4) == 0) && (y % 100))

#define SEC_PER_MIN	(60)
#define SEC_PER_HOUR	(SEC_PER_MIN * 60)
#define SEC_PER_DAY	(SEC_PER_HOUR * 24)
#define DAYSPERYEAR(y)	(IS_LEAPYEAR(y) ? 366 : 365)
#define SECPERYEAR(y)	(DAYSPERYEAR(y) * SEC_PER_DAY)

#define TODRBASE	(1 << 28) /* Rumours say it comes from VMS */

#define	SEC_OFF		0
#define	MIN_OFF		2
#define	HR_OFF		4
#define	WDAY_OFF	6
#define	DAY_OFF		7
#define	MON_OFF		8
#define	YR_OFF		9
#define	CSRA_OFF	10
#define	CSRB_OFF	11
#define	CSRD_OFF	13

#define	CSRA_UIP	0200
#define	CSRB_SET	0200
#define	CSRB_24		0002
#define	CSRB_DM		0004
#define	CSRD_VRT	0200

/* Var's used when dealing with clock chip */
extern	volatile short *clk_page;
extern	int clk_adrshift, clk_tweak;

/* Prototypes */
void	icr_hardclock(struct clockframe *);
int	generic_clkread(struct timespec *, time_t);
void	generic_clkwrite(void);
int	chip_clkread(struct timespec *, time_t);
void	chip_clkwrite(void);

int	yeartonum(int);
int	numtoyear(int);
