/*-
 * Copyright (c) 2002 Steve Murphree, Jr.
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
 *      This product includes software developed by Steve Murphree, Jr.
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
 */

#if defined(LIBC_SCCS)
	.text
	.string "$OpenBSD: setjmp.S,v 1.2 2002/09/18 05:03:54 smurph Exp $\0"
#endif /* LIBC_SCCS */

#include "SYS.h"

/*
 * C library -- setjmp, longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	setjmp(a)
 * by restoring registers from the stack,
 * and a struct sigcontext, see <signal.h>
 *
 * For m88k, we define our jmp_buf length 
 * to be the size of 22 longs.  <machine/setjmp.h>
 * The buffer's usage is as follows:
 * 
 * jmp_buf[0]		return address
 * jmp_buf[1]		signal set (if used)
 * jmp_buf[2 to 19]	r14 to r31
 * jmp_buf[20]		'used' flag
 * jmp_buf[21]		setjmp type 
 *
 */

#include <machine/setjmp.h>

/*
int setjmp(jmp_buf env);
 */
ENTRY(setjmp)
	st	r1, r2,0	/* save registers to the environment buffer */
	st	r14,r2,8
	st	r15,r2,12
	st	r16,r2,16
	st	r17,r2,20
	st	r18,r2,24
	st	r19,r2,28
	st	r20,r2,32
	st	r21,r2,36
	st	r22,r2,40
	st	r23,r2,44
	st	r24,r2,48
	st	r25,r2,52
	st	r26,r2,56
	st	r27,r2,60
	st	r28,r2,64
	st	r29,r2,68
	st	r30,r2,72
	st	r31,r2,76
	st	r0,r2,80	/* mark environment as NOT returned (0x0) */
	or	r4,r0,0		/* clear r4 */
	or	r4,r0,SETJMP_TYPE1	/* r4 now contains setjmp type */
	st	r4,r2,84	/* setjmp type to _setjmp */
	or	r15,r1,0	/* store return address in r15 */
	or	r14,r2,0	/* store address of env in r14 */
	bsr.n	_C_LABEL(sigblock) /* r2 = sigblock(0) */
	or	r2,r0,0
	st	r2,r14,4	/* save signal set in offset 4 of env */
	jmp.n	r15		/* return 0 */
	or	r2,r0,0

/*
void longjmp(jmp_buf env, int retval);
 */
ENTRY(longjmp)
	cpm	r4,r2,0x0	/* check for bad envnvironment buffer address. */
	bb1	eq,r4,2f	/* if == 0, abort. */
	ld	r4,r2,80	/* check if environment buffer has */
	cpm	r4,r4,0x0	/* already returned. */
	bb1	ne,r4,2f	/* if != 0, abort. */
	ld	r4,r2,84	/* check setjmp type. */
	cpm	r4,r4,SETJMP_TYPE1	/* should be SETJMP_TYPE1 */
	bb1	ne,r4,2f	/* if != SETJMP_TYPE1, abort. */
	
	subu	r31,r31,32	/* get a temporary stack */
	ld	r14,r2,8	/* restore registers from the environment buffer */
	ld	r15,r2,12
	ld	r16,r2,16
	ld	r17,r2,20
	ld	r18,r2,24
	ld	r19,r2,28
	ld	r20,r2,32
	ld	r21,r2,36
	ld	r22,r2,40
	ld	r23,r2,44
	ld	r24,r2,48
	ld	r25,r2,52
	ld	r26,r2,56
	ld	r27,r2,60
	ld	r28,r2,64
	ld	r29,r2,68
	
	st	r2,r31,24	/* save r2 on stack (environment) */
	st	r3,r31,28	/* save r3 on stack (return val) */
	bsr.n	_C_LABEL(sigsetmask) /* restore the signal set */
	ld	r2,r2,4
	ld	r2,r31,24	/* restore r2 from stack */
	ld	r3,r31,28	/* restore r3 from stack */
	ld	r30,r2,72	/* restore r30 */
	ld	r31,r2,76	/* restore r31 (sp)*/
	or	r4,r0,1
	st	r4,r2,80	/* mark environment buffer as returned */
	ld	r1,r2,0		/* restore r1 */
	cmp	r2,r3,0		/* test retval for 0 */
	bb0	eq,r2,1f	/* if != 0, it's ok. */
	jmp.n	r1		/* and jump to it */
	or	r2,r0,r3	/* but first return r3 value */
1:	jmp.n	r1
	or	r2,r0,1		/* return non-zero value. */
2:
	subu	r31,r31,32	/* get a temporary stack */
	st	r1,r31,20	/* save r1 on stack (return address) */
	bsr	_C_LABEL(longjmperror)
	bsr	_C_LABEL(abort)	/* NO RETURN */
	ld	r1,r31,20	/* restore r1 from stack */
	addu	r31,r31,32	/* restore the stack */
	jmp	r1	/* this should not happen but we are prepared */

