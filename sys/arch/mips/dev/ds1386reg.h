/*	$NetBSD: ds1386reg.h,v 1.1 1995/05/04 19:31:18 cgd Exp $	*/

/*
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
 * Definitions for the Dallas Semiconductor DS1386 Real Time Clock.
 *
 */

/*
 * The registers, and the bits within each register.
 */

#define	DS_SEC_100	0x0	/* Time of year: hundreds seconds (0-99) */
#define	DS_SEC		0x1	/* Time of year: seconds (0-59) */
#define	DS_MIN		0x2	/* Time of year: minutes (0-59) */
#define	DS_HOUR		0x4	/* Time of year: hour (see above) */
#define	DS_DOW		0x6	/* Time of year: day of week (1-7) */
#define	DS_DOM		0x8	/* Time of year: day of month (1-31) */
#define	DS_MONTH	0x9	/* Time of year: month (1-12) */
#define	DS_YEAR		0xa	/* Time of year: year in century (0-99) */
#define	DS_AMIN		0x3	/* Alarm: minutes */
#define	DS_AHOUR	0x5	/* Alarm: hour */
#define	DS_ADOW		0x7	/* Alarm: day */



#define	 DS_HOUR_12	0x40	/* 12-hour mode. In DS_HOUR reg */
#define	 DS_EOSC	0x80	/* Enable TOD osc if 0. In DS_MONTH reg. */

#define	DS_REGC		0xb	/* Control register */

#define	DS_REGC_TE	0x80	/* Transfer enable bit. 0 freezes regs */
#define	DS_REGC_IPSW	0x40	/* Interrupt routing bit */
#define	DS_REGC_WAM	0x08	/* Watchdog alarm int enab. 1 enables */
#define	DS_REGC_TDM	0x04	/* Time Of Day alarm int enab. 1 enables */
#define	DS_REGC_WAF	0x02	/* Watchdog alarm int status */
#define	DS_REGC_TDF	0x01	/* Time Of Day alarm int status */


#define	DS_NTODREGS	0xb	/* 10 of those regs are for TOD and alarm */

#define	DS_NVRAM_START	0xe	/* start of NVRAM: offset 14 */
#define	DS_NVRAM_SIZE	(0x8000-0xe)	/* 32k of NVRAM */

/*
 * Special NVRAM locations.
 */
#define	DS_ETHERADR	0x115	/* Ethernet address */

/*
 * RTC register/NVRAM read and write functions -- machine-dependent.
 * Appropriately manipulate RTC registers to get/put data values.
 */
u_int ds1386_read __P((void *sc, u_int reg));
void ds1386_write __P((void *sc, u_int reg, u_int datum));

/*
 * A collection of TOD/Alarm registers.
 */
typedef u_int ds_todregs[DS_NTODREGS];

/*
 * Get all of the TOD/Alarm registers
 * Must be called at splhigh(), and with the RTC properly set up.
 */
#define DS1386_GETTOD(sc, regs)						\
	do {								\
		int i;							\
									\
		/* wait for update; spin loop */			\
		i = ds1386_read(sc, DS_SEC_100);			\
		while (ds1386_read(sc, DS_SEC_100) == i)		\
			;						\
									\
		/* read all of the tod/alarm regs */			\
		for (i = 0; i < DS_NTODREGS; i++)			\
			(*regs)[i] = ds1386_read(sc, i);		\
	} while (0);

/*
 * Set all of the TOD/Alarm registers
 * Must be called at splhigh(), and with the RTC properly set up.
 */
#define DS1386_PUTTOD(sc, regs)						\
	do {								\
		int i;							\
									\
		/* stop updates while setting */			\
		ds1386_write(sc, DS_MONTH, DS_EOSC);			\
									\
		/* write all of the tod/alarm regs */			\
		for (i = 0; i < DS_NTODREGS; i++)			\
			ds1386_write(sc, i, (*regs)[i]);		\
									\
	} while (0);
