/*	$OpenBSD: dp857xreg.h,v 1.3 2003/06/09 16:34:22 deraadt Exp $ */

/*
 * Copyright (c) 1996 Per Fogelstrom
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#if !defined(_DP857X_H)
#define	_DP857X_H

/*
 *	Definition of Real Time Clock address space.
 *
 *	Clock is a National DP8570A RTC
 */
#define BSIZE           1       /* No of Bytes for Address Spacing */
#define	MAIN_STATUS	0x00	/* Main status register		*/
/*
 *  Registers selected with BS=0 RS=0
 */
#define	TIMER0_CTRL	0x01	/* Timer 0 control register	*/
#define	TIMER1_CTRL	0x02	/* Timer 0 control register	*/
#define	PERIODIC_FLAGS	0x03	/* Timer periodic flag register	*/
#define	INTERRUPT_ROUT	0x04	/* Interrupt routing register	*/
/*
 *  Registers selected with BS=0 RS=1
 */
#define	REAL_TIME_MODE	0x01	/* Real Time Mode register	*/
#define	OUTPUT_MODE	0x02	/* Output mode register		*/
#define	INTERRUPT_CTRL0	0x03	/* Interrupt control register 0	*/
#define	INTERRUPT_CTRL1	0x04	/* Interrupt control register 1	*/
/*
 *  Clock and timer registers when BS=0
 */
#define	CLK_SUBSECONDS	0x05	/* 1/100 seconds reister	*/
#define	CLK_SECONDS	0x06	/* Seconds			*/
#define	CLK_MINUTES	0x07	/* Minutes			*/
#define	CLK_HOURS	0x08	/* Hours			*/
#define	CLK_DAY		0x09	/* Day of month			*/
#define	CLK_MONTH	0x0a	/* Month			*/
#define	CLK_YEAR	0x0b	/* Year				*/
#define	CLK_JULIAN_L	0x0c	/* Lsb of Julian date		*/
#define	CLK_JULIAN_H	0x0d	/* Msb of Julian date		*/
#define	CLK_WEEKDAY	0x0e	/* Day of week			*/
#define	TIMER0_LSB	0x0f	/* Timer 0 lsb			*/
#define	TIMER0_MSB	0x10	/* Timer 0 msb			*/
#define	TIMER1_LSB	0x11	/* Timer 1 lsb			*/
#define	TIMER1_MSB	0x12	/* Timer 1 msb			*/
#define	CMP_SECONDS	0x13	/* Seconds compare		*/
#define	CMP_MINUTES	0x14	/* Minutes compare		*/
#define	CMP_HOUR	0x15	/* Hours compare		*/
#define	CMP_DAY		0x16	/* Day of month compare		*/
#define	CMP_MONTH	0x17	/* Month compare		*/
#define	CMP_WEEKDAY	0x18	/* Day of week compare		*/
#define	SAVE_SECONDS	0x19	/* Seconds time save		*/
#define	SAVE_MINUTES	0x1a	/* Minutes time save		*/
#define	SAVE_HOUR	0x1b	/* Hours time save		*/
#define	SAVE_DAY	0x1c	/* Day of month time save	*/
#define	SAVE_MONTH	0x1d	/* Month time save		*/
#define	RAM_1E		0x1e	/* Ram location 1e		*/
#define	RAM_1F		0x1f	/* Ram location 1f		*/
#define	SIZE_DP857X	0x20	/* Size of dp address map	*/

#define	DP_FIRSTTODREG	CLK_SUBSECONDS
#define	DP_LASTTODREG	CLK_WEEKDAY

typedef u_int dp_todregs[SIZE_DP857X];
u_int dp857x_read(void *sc, u_int reg);
void dp857x_write(void *sc, u_int reg, u_int datum);

/*
 * Get all of the TOD/Alarm registers
 * Must be called at splhigh(), and with the RTC properly set up.
 */
#define DP857X_GETTOD(sc, regs)					\
	do {								\
		int i;							\
									\
		/* make sure clock regs are selected */			\
		dp857x_write(sc, MAIN_STATUS, 0);			\
		/* try read until no rollover */			\
		do {							\
			/* read all of the tod/alarm regs */		\
			for (i = DP_FIRSTTODREG; i < SIZE_DP857X; i++)	\
				(*regs)[i] = dp857x_read(sc, i);	\
		} while(dp857x_read(sc, PERIODIC_FLAGS) & 7);		\
	} while (0);

/*
 * Set all of the TOD/Alarm registers
 * Must be called at splhigh(), and with the RTC properly set up.
 */
#define DP857X_PUTTOD(sc, regs)					\
	do {								\
		int i;							\
									\
		/* stop updates while setting, eg clear start bit */	\
		dp857x_write(sc, MAIN_STATUS, 0x40);			\
		dp857x_write(sc, REAL_TIME_MODE,			\
		    dp857x_read(sc, REAL_TIME_MODE) & 0xF7);		\
									\
		/* write all of the tod/alarm regs */			\
		for (i = DP_FIRSTTODREG; i <= DP_LASTTODREG; i++)	\
			dp857x_write(sc, i, (*regs)[i]);		\
									\
		/* reenable updates, eg set clock start bit */		\
		dp857x_write(sc, REAL_TIME_MODE,			\
		    dp857x_read(sc, REAL_TIME_MODE) | 0x08);		\
	} while (0);

#endif	/*_DP857X_H*/

