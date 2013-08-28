/*	$OpenBSD: _atomic_lock.c,v 1.6 2013/08/28 19:26:05 patrick Exp $	*/

/*
 * Copyright (c) 2004 Dale Rahn. All rights reserved.
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
 */ 

/*
 * Atomic lock for arm
 */

#include <sys/types.h>
#include <machine/spinlock.h>

int
_atomic_lock(volatile _atomic_lock_t *lock)
{
	_atomic_lock_t old;

#ifdef ARM_V7PLUS_LOCKS
	uint32_t scratch = 0;
	old = 0;
	__asm__("1: ldrex %0, [%1]      \n"
		"   strex %2, %3, [%1]  \n"
		"   cmp %2, #0          \n"
		"   bne 1b              \n"
		"   .long 0xf57ff05f    \n" /* XXX: use dmb */
		: "+r" (old), "+r" (lock), "+r" (scratch)
		: "r" (_ATOMIC_LOCK_LOCKED));
#else
	__asm__("swp %0, %2, [%1]"
		: "=r" (old), "=r" (lock)
		: "r" (_ATOMIC_LOCK_LOCKED), "1" (lock) );

#endif
	return (old != _ATOMIC_LOCK_UNLOCKED);
}
