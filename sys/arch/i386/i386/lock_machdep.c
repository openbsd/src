/*	$OpenBSD: lock_machdep.c,v 1.2 2004/06/13 21:49:15 niklas Exp $	*/
/* $NetBSD: lock_machdep.c,v 1.1.2.3 2000/05/03 14:40:30 sommerfeld Exp $ */

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

/*
 * Machine-dependent spin lock operations.
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/systm.h>

#include <machine/atomic.h>
#include <machine/lock.h>

#include <ddb/db_output.h>

#ifdef LOCKDEBUG

void
__cpu_simple_lock_init(lockp)
	__cpu_simple_lock_t *lockp;
{
	*lockp = __SIMPLELOCK_UNLOCKED;
}

#if defined (DEBUG) && defined(DDB)
int spin_limit = 10000000;
#endif

void
__cpu_simple_lock(lockp)
	__cpu_simple_lock_t *lockp;
{
#if defined (DEBUG) && defined(DDB)	
	int spincount = 0;
#endif
	
	while (i386_atomic_testset_i(lockp, __SIMPLELOCK_LOCKED)
	    == __SIMPLELOCK_LOCKED) {
#if defined(DEBUG) && defined(DDB)
		spincount++;
		if (spincount == spin_limit) {
			extern int db_active;
			db_printf("spundry\n");
			if (db_active) {
				db_printf("but already in debugger\n");
			} else {
				Debugger();
			}
		}
#endif
	}
}

int
__cpu_simple_lock_try(lockp)
	__cpu_simple_lock_t *lockp;
{

	if (i386_atomic_testset_i(lockp, __SIMPLELOCK_LOCKED)
	    == __SIMPLELOCK_UNLOCKED)
		return (1);
	return (0);
}

void
__cpu_simple_unlock(lockp)
	__cpu_simple_lock_t *lockp;
{
	*lockp = __SIMPLELOCK_UNLOCKED;
}

#endif
