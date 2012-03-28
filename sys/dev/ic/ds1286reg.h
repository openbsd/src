/*	$OpenBSD: ds1286reg.h,v 1.1 2012/03/28 20:44:23 miod Exp $	*/
/*	$NetBSD: ds1286reg.h,v 1.8 2005/12/11 12:21:26 christos Exp $ 	*/

/*
 * Copyright (c) 2001 Rafal K. Boni
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
 * Originally based on mc146818reg.h, with the following license:
 *
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Definitions for the Dallas Semiconductor DS1286/DS1386 Real Time Clock.
 *
 * Plucked right from the Dallas Semicomductor specs available at
 * http://pdfserv.maxim-ic.com/arpdf/DS1286.pdf and
 * http://pdfserv.maxim-ic.com/arpdf/DS1386-DS1386P.pdf
 *
 * The DS1286 and 1386 have 14 clock-related registers and some amount
 * of user registers (50 for the 1286, 8K or 32K for the 1386).  The
 * first eleven registers contain time-of-day and alarm data, the rest
 * contain various control bits and the watchdog timer functionality.
 *
 * Since the locations of these ports and the method used to access
 * them can be machine-dependent, the low-level details of reading
 * and writing the RTC's registers are handled by machine-specific
 * functions.
 *
 * The DS1286/DS1386 chips always store time-of-day and alarm data in
 * BCD.  The "hour" time-of-year and alarm fields can either be stored
 * in AM/PM format, or in 24-hour format.  If AM/PM format is chosen,
 * the hour fields can have the values: 1-12 (for AM) and 21-32 (for
 * PM).  If the 24-hour format is chosen, they can have the values 0
 * to 23.  The hour format is selectable separately for the time and
 *  alarm fields, and is controller by bit 6 of the respective register.
 */

/*
 * The registers, and the bits within each register.
 */

#define	DS1286_SUBSEC	0x0	/* Time of year: hundredths of seconds (0-99) */
#define	DS1286_SEC	0x1	/* Time of year: seconds (0-59) */
#define	DS1286_MIN	0x2	/* Time of year: minutes (0-59) */
#define	DS1286_AMIN	0x3	/* Alarm: minutes */
#define	DS1286_HOUR	0x4	/* Time of year: hour (see above) */

#define  DS1286_HOUR_12MODE	0x40	/* Hour mode: 12-hour (on), 24 (off) */
#define  DS1286_HOUR_12HR_PM	0x20	/* AM/PM in 12-hour mode: on = PM */
#define  DS1286_HOUR_12HR_MASK	0x1f	/* Mask for hours in 12hour mode */
#define  DS1286_HOUR_24HR_MASK	0x3f	/* Mask for hours in 24hour mode */

#define	DS1286_AHOUR	0x5	/* Alarm: hour */
#define	DS1286_DOW	0x6	/* Time of year: day of week (1-7) */
#define	DS1286_ADOW	0x7	/* Alarm: day of week (1-7) */
#define	DS1286_DOM	0x8	/* Time of year: day of month (1-31) */
#define	DS1286_MONTH	0x9	/* Time of year: month (1-12), wave generator */

#define	 DS1286_MONTH_MASK	0x3f	/* Mask to extract month */
#define	 DS1286_WAVEGEN_MASK	0xc0	/* Mask to extract wave bits */

#define	DS1286_YEAR	0xA	/* Time of year: year in century (0-99) */

#define	DS1286_CONTROL	0xB	/* Control register A */

#define	 DS1286_TE	0x80	/* Update in progress (on == disable update) */
#define	 DS1286_INTSWAP	0x40	/* Swap INTA, INTB outputs */
#define	 DS1286_INTBSRC	0x20	/* INTB source (on) or sink (off) current */
#define	 DS1286_INTAPLS	0x10	/* INTA pulse (on) or level (off) mode */
#define  DS1286_WAM	0x08	/* Watchdog alarm mask */
#define  DS1286_TDM	0x04	/* Time-of-day alarm mask */
#define  DS1286_WAF	0x02	/* Watchdog alarm flag */
#define  DS1286_TDF	0x01	/* Time-of-day alarm flag */

#define	DS1286_NREGS	0xd	/* 14 registers; CMOS follows */
#define	DS1286_NTODREGS	0xb	/* 11 of those regs are for TOD and alarm */

#define	DS1286_NVRAM_START 0xe	/* start of NVRAM: offset 14 */

/* NVRAM size depends on the chip -- the 1286 only has 50 bytes, whereas
 * the 1386 can have 8K or 32K
 */
#define	DS1286_NVRAM_SIZE	50		/* 50 bytes of NVRAM */

/*
 * RTC register/NVRAM read and write functions -- machine-dependent.
 * Appropriately manipulate RTC registers to get/put data values.
 */
u_int ds1286_read(void *, u_int);
void ds1286_write(void *, u_int, u_int);

/*
 * A collection of TOD/Alarm registers.
 */
typedef u_int ds1286_todregs[DS1286_NTODREGS];

/*
 * Get all of the TOD/Alarm registers
 * Must be called at splhigh(), and with the RTC properly set up.
 */
#define DS1286_GETTOD(sc, regs)						\
	do {								\
		int i;							\
		u_int ctl;						\
									\
		/* turn off update for now */				\
		ctl = ds1286_read(sc, DS1286_CONTROL);			\
		ds1286_write(sc, DS1286_CONTROL, ctl | DS1286_TE);	\
									\
		/* read all of the tod/alarm regs */			\
		for (i = 0; i < DS1286_NTODREGS; i++) 			\
			(*regs)[i] = ds1286_read(sc, i);		\
									\
		/* turn update back on */				\
		ds1286_write(sc, DS1286_CONTROL, ctl);			\
	} while (0);

/*
 * Set all of the TOD/Alarm registers
 * Must be called at splhigh(), and with the RTC properly set up.
 */
#define DS1286_PUTTOD(sc, regs)						\
	do {								\
		int i;							\
		u_int ctl;						\
									\
		/* turn off update for now */				\
		ctl = ds1286_read(sc, DS1286_CONTROL);			\
		ds1286_write(sc, DS1286_CONTROL, ctl | DS1286_TE);	\
									\
		/* write all of the tod/alarm regs */			\
		for (i = 0; i < DS1286_NTODREGS; i++) 			\
			ds1286_write(sc, i, (*regs)[i]);		\
									\
		/* turn update back on */				\
		ds1286_write(sc, DS1286_CONTROL, ctl);			\
	} while (0);
