/*	$OpenBSD: break.h,v 1.2 2002/03/15 16:41:06 jason Exp $	*/
/*
 * Copyright (c) 2002 Artur Grabowski <art@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

/*
 * Breakpoint handling.
 */
int bkpt_add_cb(struct pstate *, reg, int (*)(struct pstate *, void *), void *);
int bkpt_check(struct pstate *);
int cmd_bkpt_add(int, char **, void *);

/*
 * Single step handling.
 */
int sstep_set(struct pstate *, int (*)(struct pstate *, void *), void *);
int cmd_sstep(int, char **, void *);

/*
 * Return values from the bkpt_fun
 */
#define BKPT_DEL_STOP	1	/* delete this bkpt and stop */
#define BKPT_DEL_CONT	2	/* delete this bkpt and continue */
#define BKPT_KEEP_STOP	3	/* keep this bkpt and stop */
#define BKPT_KEEP_CONT	4	/* keep this bkpt and cont */
