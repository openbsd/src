/*	$OpenBSD: clock_subr.h,v 1.1 1997/09/09 13:08:18 maja Exp $	*/
/*	$NetBSD: clock_subr.h,v 1.2 1997/03/15 18:11:17 is Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

/*
 * "POSIX time" to/from "YY/MM/DD/hh/mm/ss"
 */
struct clock_ymdhms {
	u_short dt_year;
	u_char dt_mon;
	u_char dt_day;
	u_char dt_wday;	/* Day of week */
	u_char dt_hour;
	u_char dt_min;
	u_char dt_sec;
};

time_t clock_ymdhms_to_secs __P((struct clock_ymdhms *));
void clock_secs_to_ymdhms __P((time_t, struct clock_ymdhms *));

/*
 * BCD to decimal and decimal to BCD.
 */
#define	FROMBCD(x)	(((x) >> 4) * 10 + ((x) & 0xf))
#define	TOBCD(x)	(((x) / 10 * 16) + ((x) % 10))

/* Some handy constants. */
#define SECDAY		86400L
#define SECYR		(SECDAY * 365)

