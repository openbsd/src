/* $arla: process.alpha.S,v 1.1 2000/01/02 02:11:15 lha Exp $ */

/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

#include <config.h>

#undef RCSID

/* Code for DEC Alpha architecture */
#ifdef	AFS_OSF_ENV
#include <machine/asm.h>
#include <machine/regdef.h>
#define	fs0	$f2
#define	fs1	$f3
#define	fs2	$f4
#define	fs3	$f5
#define	fs4	$f6
#define	fs5	$f7
#define	fs6	$f8
#define	fs7	$f9
#elif AFS_LINUX_ENV
#define v0	$0
#define t0	$1
#define t1	$2		
#define t2	$3		
#define t3	$4		
#define t4	$5		
#define t5	$6		
#define t6	$7		
#define t7	$8		

#define s0      $9
#define s1	$10
#define s2	$11
#define s3	$12
#define s4	$13
#define s5	$14
#define s6	$15
	
#define fp	$15
#define a0	$16
#define a1	$17
#define a2	$18
#define a3	$19
#define a4	$20
#define a5	$21
#define t8	$22
#define t9	$23
#define t10	$24	
#define t11	$25
#define ra	$26
#define pv      $27
#define t12	$27	
#define at	$28
#define gp	$29
#define sp	$30
#define zero	$31

#define fs0     $f2
#define fs1     $f3
#define fs2     $f4
#define fs3     $f5
#define fs4     $f6
#define fs5     $f7
#define fs6     $f8
#define fs7     $f9

#define M_S0            0x00000200
#define M_S1            0x00000400
#define M_S2            0x00000800
#define M_S3            0x00001000
#define M_S4            0x00002000
#define M_S5            0x00004000
#define M_S6            0x00008000

#define  M_RA           0x04000000

#define END(proc)                                       \
	        .end    proc

#define NESTED(x, fsize, rpc)                           \
	        .globl  x	;                       \
	        .ent    x,0	;                       \
x:				;                       \
	        .frame  sp,fsize, rpc

	
#define LEAF(x)                                         \
	        .globl  x	;                       \
	        .ent    x,0	;                       \
x:				;                       \
	        .frame  sp,0,ra

#define IMPORT(sym, size)                               \
	        .extern sym,size
#elif defined(HAVE_MACHINE_ASM_H)	/* BSD */
#include <machine/asm.h>
#elif defined(HAVE_MACH_ALPHA_ASM_H)	/* OSF */
#include <mach/alpha/asm.h>
#endif

#define FRAMESIZE ((8*8)+8+(7*8))
#define floats 0
#define registers (floats+(8*8))
#define returnaddr (FRAMESIZE-8)
#define topstack 0

#if defined(AFS_OSF_ENV) || defined(AFS_LINUX_ENV)
	IMPORT(PRE_Block,4)
#endif		
.align	4
#if defined(AFS_OSF_ENV) || defined(AFS_LINUX_ENV)
NESTED(savecontext,FRAMESIZE,ra)
#else	/* OSF || LINUX */
NESTED(savecontext,3,FRAMESIZE,ra,0x0400f700,0x000003fc)
#endif	/* OSF */
	ldgp	gp,0(pv)
	lda	t0, 1(zero)
	stl	t0, PRE_Block
	lda	sp,-FRAMESIZE(sp)
/* Save callee-saved registers. */
	stq	s0, (registers+0) (sp)
	stq	s1, (registers+8) (sp)
	stq	s2, (registers+16) (sp)
	stq	s3, (registers+24) (sp)
	stq	s4, (registers+32) (sp)
	stq	s5, (registers+40) (sp)
	stq	s6, (registers+48) (sp)
/* Save return address */
	stq	ra, returnaddr(sp)

#if !defined(AFS_BSD_ENV)
	.mask	(M_S0|M_S1|M_S2|M_S3|M_S4|M_S5|M_S6|M_RA), -FRAMESIZE
#endif

/* Save floating point registers */
	stt	fs0, (floats+0) (sp)
	stt	fs1, (floats+8) (sp)
	stt	fs2, (floats+16) (sp)
	stt	fs3, (floats+24) (sp)
	stt	fs4, (floats+32) (sp)
	stt	fs5, (floats+40) (sp)
	stt	fs6, (floats+48) (sp)
	stt	fs7, (floats+56) (sp)

	.prologue	1
	stq	sp, topstack(a1)
	or	a0,zero,pv		/* call point in pv */
	beq	a2, samestack
	or	a2,zero,sp		/* switch stack */
samestack:
	jsr	ra,(pv),0		/* off we go */
	END(savecontext)

#if defined(AFS_OSF_ENV) || defined(AFS_LINUX_ENV)
LEAF(returnto)
#else	
LEAF(returnto,1)
#endif	
	ldgp	gp,0(pv)

	.prologue	1
	ldq	sp, topstack(a0)
/* Restore callee-saved regs */
	ldq	s0, (registers+0) (sp)
	ldq	s1, (registers+8) (sp)
	ldq	s2, (registers+16) (sp)
	ldq	s3, (registers+24) (sp)
	ldq	s4, (registers+32) (sp)
	ldq	s5, (registers+40) (sp)
	ldq	s6, (registers+48) (sp)
/* Return address */
	ldq	ra, returnaddr(sp)
/* Floating point registers */
	ldt	fs0, (floats+0) (sp)
	ldt	fs1, (floats+8) (sp)
	ldt	fs2, (floats+16) (sp)
	ldt	fs3, (floats+24) (sp)
	ldt	fs4, (floats+32) (sp)
	ldt	fs5, (floats+40) (sp)
	ldt	fs6, (floats+48) (sp)
	ldt	fs7, (floats+56) (sp)
	lda	sp, FRAMESIZE(sp)
	stl	zero, PRE_Block
	RET
	END(returnto)
