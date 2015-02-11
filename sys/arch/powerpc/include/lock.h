/*	$OpenBSD: lock.h,v 1.5 2015/02/11 00:14:11 dlg Exp $	*/
/*	$NetBSD: lock.h,v 1.8 2005/12/28 19:09:29 perry Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * Machine-dependent spin lock operations.
 */

#ifndef _POWERPC_LOCK_H_
#define _POWERPC_LOCK_H_

static __inline int
__cpu_cas(volatile unsigned long *addr, unsigned long old, unsigned long new)
{
        int success, scratch;
        __asm volatile(
            "1: lwarx   %0, 0,  %4      \n"
            "   cmpw    0, %0, %2       \n"
            "   li      %1, 1           \n"
            "   bne     0,2f            \n"
            "   stwcx.  %3, 0, %4       \n" 
            "   li      %1, 0           \n" 
            "   bne-    1b              \n"
	    "2:				\n"
            : "=&r" (scratch), "=&r" (success)
            : "r" (old), "r" (new), "r" (addr)
            : "memory");

        return success;
}
	
#endif /* _POWERPC_LOCK_H_ */
