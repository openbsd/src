/*	$NetBSD: clock.h,v 1.1 1996/07/20 17:35:42 ragge Exp $ */
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
 * Conversion structure.
 */
struct chiptime {
	long	sec;
	long	min;
	long	hour;
	long	day;
	long	mon;
	long	year;
};

/*
 * Time constants. These are unlikely to change.
 */
#define IS_LEAPYEAR(y) (((y % 4) == 0) && (y % 100))

#define SEC_PER_MIN	(60)
#define SEC_PER_HOUR	(SEC_PER_MIN * 60)
#define SEC_PER_DAY	(SEC_PER_HOUR * 24)
#define DAYSPERYEAR(y)	(IS_LEAPYEAR(y) ? 366 : 365)
#define SECPERYEAR(y)	(DAYSPERYEAR(y) * SEC_PER_DAY)

#define CLKREAD_OK	0
#define CLKREAD_BAD	-1
#define CLKREAD_WARN	-2

#define TODRBASE	(1 << 28) /* Rumours says it comes from VMS */

/* Prototypes */
long	chiptotime __P((struct chiptime *));
void	timetochip __P((struct chiptime *));
void	generic_clock __P((void));
void	no_nicr_clock __P((void));
int	generic_clkread __P((time_t));
void	generic_clkwrite __P((void));
