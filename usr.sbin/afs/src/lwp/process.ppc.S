/* $arla: process.ppc.S,v 1.3 2000/10/14 11:12:37 lha Exp $ */

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


/* Comments:
 *    1. Registers R10..R31 and CR0..CR7 are saved
 *    2. "struct savearea" must hold at least 3 pointers (long)
 *    3. This code will only work on 32 bit machines (601..604), not 620
 *    4. No floating point registers are saved
 *    5. The save stack "frame" is bigger than absolutely necessary.  The
 *       PowerPC [AIX] ABI needs this extra space.
 */

/*
 * PowerPC Stack structure (according to apple)
 *
 * -----------------------------------------
 * | Parameter area
 * |     [...]
 * -----------------------------------------
 * | Linkage area: (each row is 4 bytes)
 * |  Saved GPR 2 [saved by calling if imported routine,or pointer-based call]
 * |  Reserved
 * |  Reserved
 * |  Saved LR	[ may be set by called ]
 * |  Saved CR	[ may be set by called ]
 * |  Saved SP	[ saved by calling ]
 * -----------------------------------------  <-- SP
 * | Local variables
 * |     [...]
 * -----------------------------------------
 * | Parameter area
 * |     [...]
 * -----------------------------------------
 * | Linkage area:
 * |     [...]
 * -----------------------------------------  <-- SP
 */

/* Mach-O assemblers */
#if !defined(NeXT) && !defined(__APPLE__)
#define r0    0
#define r1    1
#define r2    2
#define r3    3
#define r4    4
#define r5    5
#define r6    6
#define r7    7
#define r8    8
#define r9    9
#define r10   10
#define r11   11
#define r12   12
#define r13   13
#define r14   14
#define r15   15
#define r16   16
#define r17   17
#define r18   18
#define r19   19
#define r20   20
#define r21   21
#define r22   22
#define r23   23
#define r24   24
#define r25   25
#define r26   26
#define r27   27
#define r28   28
#define r29   29
#define r30   30
#define r31   31
#endif /* !NeXT && !__APPLE__ */


/*
 * savecontext(int (*f)(), struct savearea *save, char *newsp)
 */

#define FRAME_SIZE    (32*4)+(8*4)
#define FRAME_OFFSET  (8*4)

#define TOP_OF_STACK  (0*4)
#define RETURN                (1*4)
#define CCR           (2*4)

#if defined(NeXT) || defined(__APPLE__)
      .globl  _savecontext
_savecontext:
      lis     r9,ha16(_PRE_Block)     /* Disable interrupt fiddling */
      li      r8,1
      stb     r8,lo16(_PRE_Block)(r9)
#else	
      .globl  savecontext
savecontext:
      lis     r9,PRE_Block@ha         /* Disable interrupt fiddling */
      li      r8,1
      stb     r8,PRE_Block@l(r9)
#endif /* NeXT || __APPLE__ */
      subi    r1,r1,FRAME_SIZE
      mfcr    r9
      stw     r9,CCR(r4)
      stw     r10,10*4+FRAME_OFFSET(r1)       /* Save registers */
      stw     r11,11*4+FRAME_OFFSET(r1)
      stw     r12,12*4+FRAME_OFFSET(r1)
      stw     r13,13*4+FRAME_OFFSET(r1)
      stw     r14,14*4+FRAME_OFFSET(r1)
      stw     r15,15*4+FRAME_OFFSET(r1)
      stw     r16,16*4+FRAME_OFFSET(r1)
      stw     r17,17*4+FRAME_OFFSET(r1)
      stw     r18,18*4+FRAME_OFFSET(r1)
      stw     r19,19*4+FRAME_OFFSET(r1)
      stw     r20,20*4+FRAME_OFFSET(r1)
      stw     r21,21*4+FRAME_OFFSET(r1)
      stw     r22,22*4+FRAME_OFFSET(r1)
      stw     r23,23*4+FRAME_OFFSET(r1)
      stw     r24,24*4+FRAME_OFFSET(r1)
      stw     r25,25*4+FRAME_OFFSET(r1)
      stw     r26,26*4+FRAME_OFFSET(r1)
      stw     r27,27*4+FRAME_OFFSET(r1)
      stw     r28,28*4+FRAME_OFFSET(r1)
      stw     r29,29*4+FRAME_OFFSET(r1)
      stw     r30,30*4+FRAME_OFFSET(r1)
      stw     r31,31*4+FRAME_OFFSET(r1)
      stw     r1,TOP_OF_STACK(r4)
      cmpi    0,r5,0                          /* New stack specified? */
      mflr    r0
      stw     r0,RETURN(r4)
      mtlr    r3
      beq     L1                             /* No - don't muck with pointer */

      mr      r1,r5
L1:   blr                                     /* Return */

/*
 * returnto(struct savearea *area)
 */
#if defined(NeXT) || defined(__APPLE__)
      .globl  _returnto
_returnto:
#else	
      .globl  returnto
returnto:
#endif /* NeXT || __APPLE__ */
      lwz     r1,TOP_OF_STACK(r3)             /* Update stack pointer */
      lwz     r0,RETURN(r3)                   /* Get return address */
      mtlr    r0
      lwz     r4,CCR(r3)
      mtcrf   0xFF,r4
      lwz     r10,10*4+FRAME_OFFSET(r1)       /* Restore registers */
      lwz     r11,11*4+FRAME_OFFSET(r1)
      lwz     r12,12*4+FRAME_OFFSET(r1)
      lwz     r13,13*4+FRAME_OFFSET(r1)
      lwz     r14,14*4+FRAME_OFFSET(r1)
      lwz     r15,15*4+FRAME_OFFSET(r1)
      lwz     r16,16*4+FRAME_OFFSET(r1)
      lwz     r17,17*4+FRAME_OFFSET(r1)
      lwz     r18,18*4+FRAME_OFFSET(r1)
      lwz     r19,19*4+FRAME_OFFSET(r1)
      lwz     r20,20*4+FRAME_OFFSET(r1)
      lwz     r21,21*4+FRAME_OFFSET(r1)
      lwz     r22,22*4+FRAME_OFFSET(r1)
      lwz     r23,23*4+FRAME_OFFSET(r1)
      lwz     r24,24*4+FRAME_OFFSET(r1)
      lwz     r25,25*4+FRAME_OFFSET(r1)
      lwz     r26,26*4+FRAME_OFFSET(r1)
      lwz     r27,27*4+FRAME_OFFSET(r1)
      lwz     r28,28*4+FRAME_OFFSET(r1)
      lwz     r29,29*4+FRAME_OFFSET(r1)
      lwz     r30,30*4+FRAME_OFFSET(r1)
      lwz     r31,31*4+FRAME_OFFSET(r1)
#if defined(NeXT) || defined(__APPLE__)
      lis     r9,ha16(_PRE_Block)         /* Re-enable interrupt fiddling */
      li      r8,0
      stb     r8,lo16(_PRE_Block)(r9)
#else	
      lis     r9,PRE_Block@ha         /* Re-enable interrupt fiddling */
      li      r8,0
      stb     r8,PRE_Block@l(r9)
#endif /* NeXT || __APPLE__ */
      addi    r1,r1,FRAME_SIZE
      blr
