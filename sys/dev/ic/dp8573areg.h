/*	$OpenBSD: dp8573areg.h,v 1.2 2014/10/11 18:40:21 miod Exp $	*/
/*	$NetBSD: dp8573areg.h,v 1.1 2009/02/12 06:33:57 rumble Exp $	*/

/*
 * Copyright (c) 2003 Steve Rumble
 * Copyright (c) 2001 Erik Reid
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * National Semiconductor DP8573A Real Time Clock
 */

/* Control and Status Register Offsets and Masks */
#define	DP8573A_STATUS		0x00	/* Main Status */
#define DP8573A_STATUS_INTSTAT	0x01	/* Interrupt Status */
#define DP8573A_STATUS_PWRFAIL	0x02	/* Power Fail Interrupt */
#define DP8573A_STATUS_PERINT	0x04	/* Period Interrupt */
#define DP8573A_STATUS_ALMINT	0x08	/* Alarm Interrupt */
#define DP8573A_STATUS_REGSEL	0x40	/* Register Select */

/* Register Select = 0 */
#define	DP8573A_PFLAG		0x03	/* Periodic Flag */
#define DP8573A_PFLAG_MIN	0x01	/* Minutes */
#define DP8573A_PFLAG_10SEC	0x02	/* Ten Second */
#define DP8573A_PFLAG_SEC	0x04	/* Seconds */
#define DP8573A_PFLAG_100MIL	0x08	/* 100 Millisecond */
#define DP8573A_PFLAG_10MIL	0x10	/* 10 Millisecond */
#define DP8573A_PFLAG_MIL	0x20	/* Milliseconds */
#define DP8573A_PFLAG_OFSS	0x40	/* Oscillator Fail/Single Supply */
#define DP8573A_PFLAG_TESTMODE	0x80	/* Test Mode Enable */

#define	DP8573A_TIMESAVE_CTL	0x04	/* Time Save Control */
#define DP8573A_TIMESAVE_CTL_EN	0x80	/* Time Save Enable */

/* Register Select = 1 */
#define	DP8573A_RT_MODE		0x01	/* Real Time Mode */
#define DP8573A_RT_MODE_LYLSB	0x01	/* Leap Year LSB */
#define DP8573A_RT_MODE_LYMSB	0x02	/* Leap Year MSB */
#define DP8573A_RT_MODE_1224	0x04	/* 12(low)/24(high) Hour Mode */
#define DP8573A_RT_MODE_CLKSS	0x08	/* Clock Start(high)/Stop(low) */
#define DP8573A_RT_MODE_INTPFOP	0x10	/* Interrupt PF Operation */

#define	DP8573A_OUT_MODE	0x02	/* Output Mode */
#define DP8573A_OUT_MODE_MFOPO	0x80	/* MFO Pin as Oscillator */

#define	DP8573A_INT0_CTL	0x03	/* Interrupt Control 0 */
#define DP8573A_INT0_CTL_MIN	0x01	/* Minutes Enable */
#define DP8573A_INT0_CTL_10SEC	0x02	/* 10 Second Enable */
#define DP8573A_INT0_CTL_SEC	0x04	/* Seconds Enable */
#define DP8573A_INT0_CTL_100MIL	0x08	/* 100 Millisecond Enable */
#define DP8573A_INT0_CTL_10MIL	0x10	/* 10 Millisecond Enable */
#define DP8573A_INT0_CTL_MIL	0x20	/* Millisecond Enable */

#define	DP8573A_INT1_CTL	0x04	/* Interrupt Control 1 */
#define DP8573A_INT1_CTL_SECC	0x01	/* Second Compare Enable */
#define DP8573A_INT1_CTL_MINC	0x02	/* Minute Compare Enable */
#define DP8573A_INT1_CTL_HOURC	0x04	/* Hour Compare Enable */
#define DP8573A_INT1_CTL_DOMC	0x08	/* Day of Month Compare Enable */
#define DP8573A_INT1_CTL_MONTHC	0x10	/* Month Compare Enable */
#define DP8573A_INT1_CTL_DOWC	0x20	/* Day of Week Compare Enable */
#define DP8573A_INT1_CTL_ALMINT	0x40	/* Alarm Interrupt Enable */
#define DP8573A_INT1_CTL_PWRINT	0x80	/* Power Fail Interrupt Enable */

/* Clock Counter Offsets */
#define	DP8573A_COUNTERS	0x05	/* Start of Clock Counters */
#define DP8573A_SUBSECOND	0x05	/* 1/100 Second */
#define DP8573A_SECOND		0x06	/* Seconds */
#define DP8573A_MINUTE		0x07	/* Minutes */
#define DP8573A_HOUR		0x08	/* Hours */
#define DP8573A_DOM		0x09	/* Day of Month */
#define DP8573A_MONTH		0x0a	/* Months */
#define DP8573A_YEAR		0x0b	/* Years */
#define DP8573A_DOW		0x0e	/* Day of Week */

/* Comparison Registers */
#define DP8573A_CMP_SEC		0x13	/* Seconds */ 
#define DP8573A_CMP_MIN		0x14	/* Minutes */
#define DP8573A_CMP_HOUR	0x15	/* Hours */
#define DP8573A_CMP_DOM		0x16	/* Day of Month */
#define DP8573A_CMP_MONTH	0x17	/* Months */
#define DP8573A_CMP_DOW		0x18	/* Day of Week */
		
/* Time Save Registers */
#define DP8573A_SAVE_SEC	0x19	/* Seconds */
#define DP8573A_SAVE_MIN	0x1a	/* Minutes */
#define DP8573A_SAVE_HOUR	0x1b	/* Hours */
#define DP8573A_SAVE_DOM	0x1c	/* Day of Month */
#define DP8573A_SAVE_MONTH	0x1d	/* Months */

/* RAM Registers */
#define DP8573A_RAM_0C		0x0c	/* RAM */
#define DP8573A_RAM_1E		0x1e	/* RAM */
#define DP8573A_RAM_1F		0x1f	/* RAM */

/* 12/24 Hour Masks */
#define DP8573A_HOUR_12HR_MASK	0x1f
#define DP8573A_HOUR_24HR_MASK	0x3f

#define	DP8573A_NREG		0x20
