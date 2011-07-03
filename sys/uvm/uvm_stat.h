/*	$OpenBSD: uvm_stat.h,v 1.16 2011/07/03 18:34:14 oga Exp $	*/
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

#endif /* _UVM_UVM_STAT_H_ */
