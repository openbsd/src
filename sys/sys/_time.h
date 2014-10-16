/*	$OpenBSD: _time.h,v 1.7 2014/10/16 15:50:49 millert Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
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

#ifndef _SYS__TIME_H_
#define _SYS__TIME_H_

#define CLOCK_REALTIME			0
#define CLOCK_PROCESS_CPUTIME_ID	2
#define CLOCK_MONOTONIC			3
#define CLOCK_THREAD_CPUTIME_ID		4
#define CLOCK_UPTIME			5

#if __BSD_VISIBLE
#define	__CLOCK_USE_TICKET_LOCKS	8	/* flag for __thrsleep() */

/*
 * Per-process and per-thread clocks encode the PID or TID into the
 * high bits, with the type in the bottom bits
 */
#define __CLOCK_ENCODE(type,id)		((type) | ((id) << 12))
#define __CLOCK_TYPE(c)			((c) & 0xfff)
#define __CLOCK_PTID(c)			(((c) >> 12) & 0xfffff)
#endif

/*
 * Structure defined by POSIX 1003.1b to be like a itimerval,
 * but with timespecs. Used in the timer_*() system calls.
 */
struct  itimerspec {
	struct  timespec it_interval;	/* timer interval */
	struct  timespec it_value;	/* timer expiration */
};

#define TIMER_RELTIME	0x0	/* relative timer */
#define TIMER_ABSTIME	0x1	/* absolute timer */

#endif /* !_SYS__TIME_H_ */
