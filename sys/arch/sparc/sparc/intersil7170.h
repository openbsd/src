/*	$NetBSD: intersil7170.h,v 1.1 1994/12/16 22:17:00 deraadt Exp $	*/

/*
 * Copyright (c) 1993 Adam Glass
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
 *	This product includes software developed by Adam Glass.
 * 4. The name of the Author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Adam Glass ``AS IS'' AND
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
 */

/*
 * Driver support for the intersil7170 used in sun[34]s to provide
 * real time clock and time-of-day support.
 * 
 * Derived from: datasheet "ICM7170 a uP-Compatible Real-Time Clock"
 *                          document #301680-005, Dec 85
 */

struct date_time {		       /* from p. 7 of 10 */
	u_char dt_csec;
	u_char dt_hour;
	u_char dt_min;
	u_char dt_sec;
	u_char dt_month;
	u_char dt_day;
	u_char dt_year;
	u_char dt_dow;
};

struct intersil7170 {
	struct date_time counters;
	struct date_time clk_ram;	/* should be ok as both are word aligned */
	u_char clk_intr_reg;
	u_char clk_cmd_reg;
};

/*  bit assignments for command register, p. 6 of 10, write-only */
#define INTERSIL_CMD_FREQ_32K	0x00 
#define INTERSIL_CMD_FREQ_1M	0x01
#define INTERSIL_CMD_FREQ_2M	0x02
#define INTERSIL_CMD_FREQ_4M	0x03

#define INTERSIL_CMD_12HR_MODE	0x00
#define INTERSIL_CMD_24HR_MODE	0x04

#define INTERSIL_CMD_STOP	0x00
#define INTERSIL_CMD_RUN	0x08

#define INTERSIL_CMD_IDISABLE	0x00
#define INTERSIL_CMD_IENABLE	0x10

#define INTERSIL_CMD_TEST_MODE	 0x20
#define INTERSIL_CMD_NORMAL_MODE 0x00

/* bit assignments for interrupt register r/w, p 7 of 10*/

#define INTERSIL_INTER_ALARM	0x01	/* r/w */
#define INTERSIL_INTER_CSECONDS	0x02	/* r/w */
#define INTERSIL_INTER_DSECONDS	0x04	/* r/w */
#define INTERSIL_INTER_SECONDS	0x08	/* r/w */
#define INTERSIL_INTER_MINUTES	0x10	/* r/w */
#define INTERSIL_INTER_HOURS	0x20	/* r/w */
#define INTERSIL_INTER_DAYS	0x40	/* r/w */
#define INTERSIL_INTER_PENDING	0x80	/* read-only */

#define INTERSIL_INTER_BITS "\20\10PENDING\7DAYS\6HRS\5MIN\4SCDS\3DSEC\2CSEC\1ALARM"

