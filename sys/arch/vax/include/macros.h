/*	$NetBSD: macros.h,v 1.6 1995/12/13 18:56:01 ragge Exp $	*/

/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

 /* All bugs are subject to removal without further notice */

#if !defined(_VAX_MACROS_H_) && (defined(STANDALONE) || \
	(!defined(ASSEMBLER) && defined(_VAX_INLINE_)))
#define	_VAX_MACROS_H_

/* Here general macros are supposed to be stored */

static __inline__ int ffs(int reg){
	register int val;

	asm __volatile ("ffs	$0,$32,%1,%0
			bneq	1f
			mnegl	$1,%0
		1:	incl    %0"
			: "&=r" (val)
			: "r" (reg) );
	return	val;
}

static __inline__ void _remque(void*p){
	asm __volatile ("remque (%0),%0;clrl 4(%0)"
			:
			: "r" (p)
			: "memory" );
}

static __inline__ void _insque(void*p, void*q) {
        asm __volatile ("insque (%0), (%1)"
                        :
                        : "r" (p),"r" (q)
                        : "memory" );
}

#define	bitset(bitnr,var)				\
({	asm __volatile ("bbss %0,%1,1f;1:;"		\
			:				\
			: "g" (bitnr), "g" (var));	\
})

#define	bitclear(bitnr,var)				\
({      asm __volatile ("bbsc %0,%1,1f;1:;"             \
                        :                               \
                        : "g" (bitnr), "g" (var));      \
})

#define	bitisset(bitnr,var)				\
({							\
	register int val;                               \
	asm __volatile ("clrl %0;bbc %1,%2,1f;incl %0;1:;" \
			: "=g" (val)			\
			: "g" (bitnr), "g" (var));	\
	val;						\
})

#define bitisclear(bitnr,var)                                \
({                                                      \
        register int val;                               \
        asm __volatile ("clrl %0;bbs %1,%2,1f;incl %0;1:;" \
                        : "=g" (val)                    \
                        : "g" (bitnr), "g" (var));      \
	val;						\
})
static __inline__ void bcopy(const void*from, void*toe, u_int len) {
	asm __volatile ("movc3 %0,(%1),(%2)"
			:
			: "r" (len),"r" (from),"r"(toe)
			:"r0","r1","r2","r3","r4","r5");
}

static __inline__ void bzero(void*block, u_int len){
	asm __volatile ("movc5 $0,(%0),$0,%1,(%0)"
			:
			: "r" (block), "r" (len)
			:"r0","r1","r2","r3","r4","r5");
}

static __inline__ int bcmp(const void *b1, const void *b2, size_t len){
	register ret;

	asm __volatile("cmpc3 %3,(%1),(%2);movl r0,%0"
			: "=r" (ret)
			: "r" (b1), "r" (b2), "r" (len)
			: "r0","r1","r2","r3" );
	return ret;
}

static __inline__ int locc(int mask, char *cp,u_int size){
	register ret;

	asm __volatile("locc %1,%2,(%3);movl r0,%0"
			: "=r" (ret)
			: "r" (mask),"r"(size),"r"(cp)
			: "r0","r1" );
	return	ret;
}

static __inline__ int scanc(u_int size, u_char *cp,u_char *table, int mask){
	register ret;

	asm __volatile("scanc	%1,(%2),(%3),%4;movl r0,%0"
			: "=g"(ret)
			: "r"(size),"r"(cp),"r"(table),"r"(mask)
			: "r0","r1","r2","r3" );
	return ret;
}

static __inline__ int skpc(int mask, int size, char *cp){
	register ret;

	asm __volatile("skpc %1,%2,(%3);movl r0,%0"
			: "=g"(ret)
			: "r"(mask),"r"(size),"r"(cp)
			: "r0","r1" );
	return	ret;
}
#if 0
static __inline__ int imin(int a, int b){
	asm __volatile("cmpl %0,%2;bleq 1f;movl %2,%0;1:"
			: "=r"(a)
			: "r"(a),"r"(b) );
	return a;
}

static __inline__ int imax(int a, int b){
        asm __volatile("cmpl %0,%2;bgeq 1f;movl %2,%0;1:"
                        : "=r"(a)
                        : "r"(a),"r"(b) );
        return a;
}

static __inline__ int min(int a, int b){
        asm __volatile("cmpl %0,%2;bleq 1f;movl %2,%0;1:"
                        : "=r"(a)
                        : "r"(a),"r"(b) );
        return a;
}

static __inline__ int max(int a, int b){
        asm __volatile("cmpl %0,%2;bgeq 1f;movl %2,%0;1:"
                        : "=r"(a)
                        : "r"(a),"r"(b) );
        return a;
}
#endif

static __inline__ void blkcpy(const void*from, void*to, u_int len) {
	asm __volatile("
			movl    %0,r1
			movl    %1,r3
			movl	%2,r6
			jbr 2f
		1:	subl2   r0,r6
			movc3   r0,(r1),(r3)
		2:	movzwl  $65535,r0
			cmpl    r6,r0
			jgtr    1b
			movc3   r6,(r1),(r3)"
			:
			: "g" (from), "g" (to), "g" (len)
			: "r0","r1","r2","r3","r4","r5", "r6" );
}

static __inline__ void blkclr(void *blk, int len) {
	asm __volatile("
			movl	%0, r3
			movl	%1, r6
			jbr	2f
		1:	subl2	r0, r6
			movc5	$0,(r3),$0,r0,(r3)
		2:	movzwl	$65535,r0
			cmpl	r6, r0
			jgtr	1b
			movc5	$0,(r3),$0,r6,(r3)"
			:
			: "g" (blk), "g" (len)
			: "r0","r1","r2","r3","r4","r5", "r6" );
}

#endif	/* _VAX_MACROS_H_ */
