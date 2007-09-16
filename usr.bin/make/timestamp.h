#ifndef TIMESTAMP_H
#define TIMESTAMP_H

/*	$OpenPackages$ */
/*	$OpenBSD: timestamp.h,v 1.2 2007/09/16 12:09:36 espie Exp $ */

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
 * TIMESTAMP: 			opaque data type to store a date.
 * ts_set_out_of_date(t):	set up t so that it is out-of-date.
 * b = is_out_of_date(t):	check whether t is out-of-date.
 * ts_set_from_stat(s, t):	grab date out of stat(2) buffer.
 * b = is_strictly_before(t1, t2):	
 *				check whether t1 is before t2.
 * stamp = timestamp2time_t(t):	extract time_t from timestamp.
 * ts_set_from_time_t(d, t):	create timestamp from time_t.
 * ts_set_from_now(n):		grab current date.
 */

/* sysresult = set_times(name):	set modification times on a file. 
 * 				system call results.
 */

#define Init_Timestamp()	ts_set_from_now(now)

#ifndef TIMESTAMP_TYPE
#include "timestamp_t.h"
#endif
#ifdef USE_TIMESPEC
#define ts_set_out_of_date(t)	(t).tv_sec = INT_MIN, (t).tv_nsec = 0
#define is_out_of_date(t)	((t).tv_sec == INT_MIN && (t).tv_nsec == 0)
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
#define ts_set_from_now(n) \
do { \
	struct timeval tv; \
	gettimeofday(&tv, NULL); \
	TIMEVAL_TO_TIMESPEC(&(tv), &n); \
} while (0)
#define timestamp2time_t(t)	((t).tv_sec)
#else
#define is_out_of_date(t)	((t) == INT_MIN)
#define ts_set_out_of_date(t)	(t) = INT_MIN
#define ts_set_from_stat(s, t) \
do { \
	(t) = (s).st_mtime; \
	if (is_out_of_date(t)) \
		(t)++; \
} while (0)
#define is_strictly_before(t1, t2)	((t1) < (t2))
#define ts_set_from_time_t(d, t) \
do { \
	(t) = d; \
	if (is_out_of_date(t)) \
		(t)++; \
} while (0)
#define ts_set_from_now(n) time(&(n))
#define timestamp2time_t(t)	(t)
#endif

extern int set_times(const char *);

extern TIMESTAMP now;		/* The time at the start of this whole
				 * process */
extern char *time_to_string(TIMESTAMP t);


#endif
