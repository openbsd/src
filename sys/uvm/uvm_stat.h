/*	$OpenBSD: uvm_stat.h,v 1.13 2002/03/14 01:27:19 millert Exp $	*/
/*	$NetBSD: uvm_stat.h,v 1.19 2001/02/04 10:55:58 mrg Exp $	*/

/*
 *
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
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
 *      This product includes software developed by Charles D. Cranor and
 *      Washington University.
 * 4. The name of the author may not be used to endorse or promote products
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
 *
 * from: Id: uvm_stat.h,v 1.1.2.4 1998/02/07 01:16:56 chs Exp
 */

#ifndef _UVM_UVM_STAT_H_
#define _UVM_UVM_STAT_H_

#include <sys/queue.h>

/*
 * uvm_stat: monitor what is going on with uvm (or whatever)
 */

/*
 * counters  [XXX: maybe replace event counters with this]
 */

#define UVMCNT_MASK	0xf			/* rest are private */
#define UVMCNT_CNT	0			/* normal counter */
#define UVMCNT_DEV	1			/* device event counter */

struct uvm_cnt {
	int c;					/* the value */
	int t;					/* type */
	struct uvm_cnt *next;			/* global list of cnts */
	char *name;				/* counter name */
	void *p;				/* private data */
};

#ifdef _KERNEL

extern struct uvm_cnt *uvm_cnt_head;

/*
 * counter operations.  assume spl is set ok.
 */

#define UVMCNT_INIT(CNT,TYP,VAL,NAM,PRIV) \
do { \
	CNT.c = VAL; \
	CNT.t = TYP; \
	CNT.next = uvm_cnt_head; \
	uvm_cnt_head = &CNT; \
	CNT.name = NAM; \
	CNT.p = PRIV; \
} while (0)

#define UVMCNT_SET(C,V) \
do { \
	(C).c = (V); \
} while (0)

#define UVMCNT_ADD(C,V) \
do { \
	(C).c += (V); \
} while (0)

#define UVMCNT_INCR(C) UVMCNT_ADD(C,1)
#define UVMCNT_DECR(C) UVMCNT_ADD(C,-1)

#endif /* _KERNEL */

/*
 * history/tracing
 */

struct uvm_history_ent {
	struct timeval tv; 		/* time stamp */
	char *fmt; 			/* printf format */
	size_t fmtlen;			/* length of printf format */
	char *fn;			/* function name */
	size_t fnlen;			/* length of function name */
	u_long call;			/* function call number */
	u_long v[4];			/* values */
};

struct uvm_history {
	const char *name;		/* name of this this history */
	size_t namelen;			/* length of name, not including null */
	LIST_ENTRY(uvm_history) list;	/* link on list of all histories */
	int n;				/* number of entries */
	int f; 				/* next free one */
	simple_lock_data_t l;		/* lock on this history */
	struct uvm_history_ent *e;	/* the malloc'd entries */
};

LIST_HEAD(uvm_history_head, uvm_history);

/*
 * grovelling lists all at once.  we currently do not allow more than
 * 32 histories to exist, as the way to dump a number of them at once
 * is by calling uvm_hist() with a bitmask.
 */

/* this is used to set the size of some arrays */
#define	MAXHISTS		32	/* do not change this! */

/* and these are the bit values of each history */
#define	UVMHIST_MAPHIST		0x00000001	/* maphist */
#define	UVMHIST_PDHIST		0x00000002	/* pdhist */
#define	UVMHIST_UBCHIST		0x00000004	/* ubchist */

#ifdef _KERNEL

/*
 * macros to use the history/tracing code.  note that UVMHIST_LOG
 * must take 4 arguments (even if they are ignored by the format).
 */
#ifndef UVMHIST
#define UVMHIST_DECL(NAME)
#define UVMHIST_INIT(NAME,N)
#define UVMHIST_INIT_STATIC(NAME,BUF)
#define UVMHIST_LOG(NAME,FMT,A,B,C,D)
#define UVMHIST_CALLED(NAME)
#define UVMHIST_FUNC(FNAME)
#define uvmhist_dump(NAME)
#else
extern	struct uvm_history_head uvm_histories;

#define UVMHIST_DECL(NAME) struct uvm_history NAME

#define UVMHIST_INIT(NAME,N) \
do { \
	(NAME).name = __STRING(NAME); \
	(NAME).namelen = strlen((NAME).name); \
	(NAME).n = (N); \
	(NAME).f = 0; \
	simple_lock_init(&(NAME).l); \
	(NAME).e = (struct uvm_history_ent *) \
		malloc(sizeof(struct uvm_history_ent) * (N), M_TEMP, \
		    M_WAITOK); \
	memset((NAME).e, 0, sizeof(struct uvm_history_ent) * (N)); \
	LIST_INSERT_HEAD(&uvm_histories, &(NAME), list); \
} while (0)

#define UVMHIST_INIT_STATIC(NAME,BUF) \
do { \
	(NAME).name = __STRING(NAME); \
	(NAME).namelen = strlen((NAME).name); \
	(NAME).n = sizeof(BUF) / sizeof(struct uvm_history_ent); \
	(NAME).f = 0; \
	simple_lock_init(&(NAME).l); \
	(NAME).e = (struct uvm_history_ent *) (BUF); \
	memset((NAME).e, 0, sizeof(struct uvm_history_ent) * (NAME).n); \
	LIST_INSERT_HEAD(&uvm_histories, &(NAME), list); \
} while (0)

#if defined(UVMHIST_PRINT)
extern int uvmhist_print_enabled;
#define UVMHIST_PRINTNOW(E) \
do { \
		if (uvmhist_print_enabled) { \
			uvmhist_print(E); \
			DELAY(100000); \
		} \
} while (0)
#else
#define UVMHIST_PRINTNOW(E) /* nothing */
#endif

#define UVMHIST_LOG(NAME,FMT,A,B,C,D) \
do { \
	int _i_, _s_ = splhigh(); \
	simple_lock(&(NAME).l); \
	_i_ = (NAME).f; \
	(NAME).f = (_i_ + 1) % (NAME).n; \
	simple_unlock(&(NAME).l); \
	splx(_s_); \
	if (!cold) \
		microtime(&(NAME).e[_i_].tv); \
	(NAME).e[_i_].fmt = (FMT); \
	(NAME).e[_i_].fmtlen = strlen((NAME).e[_i_].fmt); \
	(NAME).e[_i_].fn = _uvmhist_name; \
	(NAME).e[_i_].fnlen = strlen((NAME).e[_i_].fn); \
	(NAME).e[_i_].call = _uvmhist_call; \
	(NAME).e[_i_].v[0] = (u_long)(A); \
	(NAME).e[_i_].v[1] = (u_long)(B); \
	(NAME).e[_i_].v[2] = (u_long)(C); \
	(NAME).e[_i_].v[3] = (u_long)(D); \
	UVMHIST_PRINTNOW(&((NAME).e[_i_])); \
} while (0)

#define UVMHIST_CALLED(NAME) \
do { \
	{ \
		int s = splhigh(); \
		simple_lock(&(NAME).l); \
		_uvmhist_call = _uvmhist_cnt++; \
		simple_unlock(&(NAME).l); \
		splx(s); \
	} \
	UVMHIST_LOG(NAME,"called!", 0, 0, 0, 0); \
} while (0)

#define UVMHIST_FUNC(FNAME) \
	static int _uvmhist_cnt = 0; \
	static char *_uvmhist_name = FNAME; \
	int _uvmhist_call; 

static __inline void uvmhist_print(struct uvm_history_ent *);

static __inline void
uvmhist_print(e)
	struct uvm_history_ent *e;
{
	printf("%06ld.%06ld ", e->tv.tv_sec, e->tv.tv_usec);
	printf("%s#%ld: ", e->fn, e->call);
	printf(e->fmt, e->v[0], e->v[1], e->v[2], e->v[3]);
	printf("\n");
}
#endif /* UVMHIST */

#endif /* _KERNEL */

#endif /* _UVM_UVM_STAT_H_ */
