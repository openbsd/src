#ifndef TIMESTAMP_H
#define TIMESTAMP_H

/*	$OpenBSD: timestamp.h,v 1.10 2013/05/22 12:14:08 espie Exp $ */

/*
 * Copyright (c) 2001 Marc Espie.
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
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* This module handles time stamps on files in a relatively high-level way.
 * Most of the interface is achieved through inlineable code.
 *
 * ts_set_out_of_date(t):	set up t so that it is out-of-date.
 * b = is_out_of_date(t):	check whether t is out-of-date.
 * ts_set_from_stat(s, t):	grab date out of stat(2) buffer.
 * b = is_strictly_before(t1, t2):
 *				check whether t1 is before t2.
 * ts_set_from_time_t(d, t):	create timestamp from time_t.
 */

/* sysresult = set_times(name):	set modification times on a file.
 * 				system call results.
 */

#define Init_Timestamp()	clock_gettime(CLOCK_REALTIME, &starttime)

#define TMIN (sizeof(time_t) == sizeof(int32_t) ? INT32_MIN : INT64_MIN)
#define ts_set_out_of_date(t)	(t).tv_sec = TMIN, (t).tv_nsec = 0
#define is_out_of_date(t)	((t).tv_sec == TMIN && (t).tv_nsec == 0)

#define ts_set_from_stat(s, t) \
do { \
	(t).tv_sec = (s).st_mtime; \
	(t).tv_nsec = (s).st_mtimensec; \
	if (is_out_of_date(t)) \
		(t).tv_nsec++; \
} while (0)
#define is_strictly_before(t1, t2)	timespeccmp(&(t1), &(t2), <)
#define ts_set_from_time_t(d, t) \
do { \
	(t).tv_sec = d; \
	(t).tv_nsec = 0; \
	if (is_out_of_date(t)) \
		(t).tv_nsec++; \
} while (0)

extern int set_times(const char *);

extern struct timespec starttime;	/* The time at the start 
					 * of this whole process */
extern char *time_to_string(struct timespec *);


#endif
