/*	$OpenBSD: rtc.h,v 1.4 2000/01/24 16:02:04 espie Exp $	*/
/*	$NetBSD: rtc.h,v 1.5 1997/07/17 23:29:28 is Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
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
 *      This product includes software developed by Christian E. Hopps.
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
 *
 * information on A2000 clock from Harald Backert.
 * information on A3000 clock from Holger Emden.
 */
#ifndef _RTCVAR_H_
#define _RTCVAR_H_

/* this is a hook set by a clock driver for the configured realtime clock,
   returning plain current unix-time */

time_t (*gettod) __P((void));
int (*settod) __P((time_t));

struct rtclock2000 {
	u_int  :28, second2:4;	/* lower digit */
	u_int  :28, second1:4;	/* upper digit */
	u_int  :28, minute2:4;	/* lower digit */
	u_int  :28, minute1:4;	/* upper digit */
	u_int  :28, hour2:4;	/* lower digit */
	u_int  :28, hour1:4;	/* upper digit */
	u_int  :28, day2:4;	/* lower digit */
	u_int  :28, day1:4;	/* upper digit */
	u_int  :28, month2:4;	/* lower digit */
	u_int  :28, month1:4;	/* upper digit */
	u_int  :28, year2:4;	/* lower digit */
	u_int  :28, year1:4;	/* upper digit */
	u_int  :28, weekday:4;	/* weekday */
	u_int  :28, control1:4;	/* control-byte 1 */
	u_int  :28, control2:4;	/* control-byte 2 */  
	u_int  :28, control3:4;	/* control-byte 3 */
};

/*
 * commands written to control1, HOLD before reading the clock,
 * FREE after done reading.
 */

#define A2CONTROL1_HOLD		(1<<0)
#define A2CONTROL1_BUSY		(1<<1)
#define A2CONTROL3_24HMODE	(1<<2)
#define A2HOUR1_PM		(1<<2)

struct rtclock3000 {
	u_int  :28, second2:4;	/* 0x03  lower digit */
	u_int  :28, second1:4;	/* 0x07  upper digit */
	u_int  :28, minute2:4;	/* 0x0b  lower digit */
	u_int  :28, minute1:4;	/* 0x0f  upper digit */
	u_int  :28, hour2:4;	/* 0x13  lower digit */
	u_int  :28, hour1:4;	/* 0x17  upper digit */
	u_int  :28, weekday:4;	/* 0x1b */
	u_int  :28, day2:4;	/* 0x1f  lower digit */
	u_int  :28, day1:4;	/* 0x23  upper digit */
	u_int  :28, month2:4;	/* 0x27  lower digit */
	u_int  :28, month1:4;	/* 0x2b  upper digit */
	u_int  :28, year2:4;	/* 0x2f  lower digit */
	u_int  :28, year1:4;	/* 0x33  upper digit */
	u_int  :28, control1:4;	/* 0x37  control-byte 1 */
	u_int  :28, control2:4;	/* 0x3b  control-byte 2 */  
	u_int  :28, control3:4;	/* 0x3f  control-byte 3 */
};

#define A3CONTROL1_HOLD_CLOCK	0
#define A3CONTROL1_FREE_CLOCK	9

/* mode1 registers we use */
#define leapyear year2

#define A3BBC_SET_REG 	0xe0
#define A3BBC_WRITE_REG	0xc2
#define A3BBC_READ_REG	0xc3
#define A3NUM_BBC_REGS	12

/*
 * Our clock starts at 1/1/1970, but counts the years from 1900.
 */
#define	STARTOFTIME	1970
#define	CLOCK_BASE_YEAR	1900

#endif /* _RTCVAR_H_ */
