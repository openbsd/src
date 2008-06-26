/*	$OpenBSD: intersil7170.h,v 1.2 2008/06/26 05:42:15 ray Exp $	*/
/*	$NetBSD: intersil7170.h,v 1.1 1997/05/02 06:15:28 jeremy Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_INTERSIL7170_H
#define	_INTERSIL7170_H

/*
 * Driver support for the intersil7170 used in sun[34]s to provide
 * real time clock and time-of-day support.
 *
 * Derived from: datasheet "ICM7170 a uP-Compatible Real-Time Clock"
 *                          document #301680-005, Dec 85
 */

struct intersil_dt {		       /* from p. 7 of 10 */
    u_int8_t dt_csec;
    u_int8_t dt_hour;
    u_int8_t dt_min;
    u_int8_t dt_sec;
    u_int8_t dt_month;
    u_int8_t dt_day;
    u_int8_t dt_year;
    u_int8_t dt_dow;
};

struct intersil7170 {
    struct intersil_dt counters;
    struct intersil_dt clk_ram;	/* should be ok as both are word aligned */
    u_int8_t clk_intr_reg;
    u_int8_t clk_cmd_reg;
};

/*  bit assignments for command register, p. 6 of 10, write-only */
#define INTERSIL_CMD_FREQ_32K    0x0
#define INTERSIL_CMD_FREQ_1M     0x1
#define INTERSIL_CMD_FREQ_2M     0x2
#define INTERSIL_CMD_FREQ_4M     0x3

#define INTERSIL_CMD_12HR_MODE   0x0
#define INTERSIL_CMD_24HR_MODE   0x4

#define INTERSIL_CMD_STOP        0x0
#define INTERSIL_CMD_RUN         0x8

#define INTERSIL_CMD_IDISABLE   0x0
#define INTERSIL_CMD_IENABLE   0x10

#define INTERSIL_CMD_TEST_MODE      0x20
#define INTERSIL_CMD_NORMAL_MODE    0x0

/* bit assignments for interrupt register r/w, p 7 of 10*/

#define INTERSIL_INTER_ALARM       0x1 /* r/w */
#define INTERSIL_INTER_CSECONDS    0x2 /* r/w */
#define INTERSIL_INTER_DSECONDS    0x4 /* r/w */
#define INTERSIL_INTER_SECONDS     0x8 /* r/w */
#define INTERSIL_INTER_MINUTES    0x10 /* r/w */
#define INTERSIL_INTER_HOURS      0x20 /* r/w */
#define INTERSIL_INTER_DAYS       0x40 /* r/w */
#define INTERSIL_INTER_PENDING    0x80 /* read-only */

#define INTERSIL_INTER_BITS "\20\10PENDING\7DAYS\6HRS\5MIN\4SCDS\3DSEC\2CSEC\1ALARM"

#endif	/* _INTERSIL7170_H */
