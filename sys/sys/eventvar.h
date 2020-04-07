/*	$OpenBSD: eventvar.h,v 1.9 2020/04/07 13:27:52 visa Exp $	*/

/*-
 * Copyright (c) 1999,2000 Jonathan Lemon <jlemon@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD: src/sys/sys/eventvar.h,v 1.3 2000/05/26 02:06:54 jake Exp $
 */

#ifndef _SYS_EVENTVAR_H_
#define _SYS_EVENTVAR_H_

#include <sys/task.h>

#define KQ_NEVENTS	8		/* minimize copy{in,out} calls */
#define KQEXTENT	256		/* linear growth by this amount */

/*
 * Locking:
 *	a	atomic operations
 */
struct kqueue {
	TAILQ_HEAD(, knote) kq_head;		/* list of pending event */
	int		kq_count;		/* number of pending events */
	u_int		kq_refs;		/* [a] number of references */
	struct		selinfo kq_sel;
	struct		filedesc *kq_fdp;

	LIST_ENTRY(kqueue) kq_next;

	int		kq_knlistsize;		/* size of kq_knlist */
	struct		knlist *kq_knlist;	/* list of attached knotes */
	u_long		kq_knhashmask;		/* size of kq_knhash */
	struct		knlist *kq_knhash;	/* hash table for attached knotes */
	struct		task kq_task;		/* deferring of activation */

	int		kq_state;
#define KQ_SEL		0x01
#define KQ_SLEEP	0x02
#define KQ_DYING	0x04
};

#endif /* !_SYS_EVENTVAR_H_ */
