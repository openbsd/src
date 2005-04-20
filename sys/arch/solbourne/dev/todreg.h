/*	$OpenBSD: todreg.h,v 1.1 2005/04/20 01:00:16 miod Exp $	*/
/*
 * Copyright (c) 2005, Miodrag Vallat
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
 * Oki MSM62X42BRS registers.
 *
 * A datasheet for this chip is available from:
 *   http://www.datasheetarchive.com/datasheet/pdf/19/196099.html
 */

#define	MSM_REG(x)		((x) << 3)

#define	MSM_SEC_UNITS		MSM_REG(0)	/* seconds, low digit */
#define	MSM_SEC_TENS		MSM_REG(1)	/* seconds, high digit */
#define	MSM_MIN_UNITS		MSM_REG(2)	/* minutes, low digit */
#define	MSM_MIN_TENS		MSM_REG(3)	/* minutes, high digit */
#define	MSM_HOUR_UNITS		MSM_REG(4)	/* hours, low digit */
#define	MSM_HOUR_TENS		MSM_REG(5)	/* hours, high digit */
#define	MSM_HOUR_PM		0x04		/* PM bit if PM mode */
#define	MSM_DAY_UNITS		MSM_REG(6)	/* day, low digit */
#define	MSM_DAY_TENS		MSM_REG(7)	/* day, high digit */
#define	MSM_MONTH_UNITS		MSM_REG(8)	/* month, low digit */
#define	MSM_MONTH_TENS		MSM_REG(9)	/* month, high digit */
#define	MSM_YEAR_UNITS		MSM_REG(10)	/* year, low digit */
#define	MSM_YEAR_TENS		MSM_REG(11)	/* year, high digit */
#define	CLOCK_YEAR_BASE		1968
#define	MSM_DOW			MSM_REG(12)	/* day of week, 0 = sunday */

#define	MSM_D			MSM_REG(13)	/* control register D */
#define	MSM_D_HOLD		0x01	/* hold clock for access */
#define	MSM_D_BUSY		0x02	/* clock is busy */
#define	MSM_D_INTR		0x04	/* interrupt pending */
#define	MSM_D_30		0x08	/* 30 seconds adjustment */

#define	MSM_E			MSM_REG(14)	/* control register E */
#define	MSM_E_MASK		0x01	/* output mask (set to disable) */
#define	MSM_E_INTR		0x02	/* output interrupts (1) or pulse (0) */
#define	MSM_E_PERIOD		0x04	/* interrupt (or pulse) period - needs
					   to be written twice! (2 bits) */
#define	MSM_PERIOD_64HZ		0x00
#define	MSM_PERIOD_1HZ		0x01
#define	MSM_PERIOD_1MIN		0x10
#define	MSM_PERIOD_1HOUR	0x11

#define	MSM_F			MSM_REG(15)	/* control register F */
#define	MSM_F_RESET		0x01	/* reset clock */
#define	MSM_F_STOP		0x02	/* stop clock */
#define	MSM_F_24HR		0x04	/* 24 hour mode (valid on reset only) */
#define	MSM_F_TEST		0x08	/* assert test signal */
