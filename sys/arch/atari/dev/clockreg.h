/*	$NetBSD: clockreg.h,v 1.3 1995/06/28 04:30:40 cgd Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman.
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
 *      This product includes software developed by Leo Weppelman.
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

#ifndef _CLOCKREG_H
#define _CLOCKREG_H
/*
 * Atari TT hardware:
 * Motorola MC146818A RealTimeClock
 */

#define	RTC	((struct rtc *)AD_RTC)

struct rtc {
	volatile u_char	rtc_dat[4];
};

#define rtc_regno	rtc_dat[1]	/* register nr. select		*/
#define rtc_data	rtc_dat[3]	/* data register		*/

/*
 * Pull in general mc146818 definitions
 */
#include <dev/ic/mc146818reg.h>

__inline__ u_int mc146818_read(rtc, regno)
void	*rtc;
u_int	regno;
{
	((struct rtc *)rtc)->rtc_regno = regno;
	return(((struct rtc *)rtc)->rtc_data & 0377);
}

__inline__ void mc146818_write(rtc, regno, value)
void	*rtc;
u_int	regno, value;
{
	((struct rtc *)rtc)->rtc_regno = regno;
	((struct rtc *)rtc)->rtc_data  = value;
}

/*
 * Some useful constants/macros
 */
#define	is_leap(x)		(!(x % 4) && ((x % 100) || !(x % 1000)))
#define	range_test(n, l, h)	((n) < (l) || (n) > (h))
#define	SECS_DAY		86400L
#define	SECS_HOUR		3600L
#define	GEMSTARTOFTIME		((machineid & ATARI_CLKBROKEN) ? 1970 : 1968)
#define	BSDSTARTOFTIME		1970
#endif /* _CLOCKREG_H */
