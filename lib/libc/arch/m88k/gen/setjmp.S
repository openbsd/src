#include "SYS.h"

ENTRY(setjmp)
	st	r1, r2,0
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
	or	r15,r1,0	/* can use r15 */
	or	r14,r2,0	/* can use r14 as it is already saved */
	bsr.n	_sigblock
	or	r2,r0,0
	st	r2,r14,4
	jmp.n	r15
	or	r2,r0,0

ENTRY(longjmp)
	subu	r31,r31,32		/* get some temporary stack */

	ld	r14,r2,8
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

	st	r2,r31,24		/* save r2 on stack */
	st	r3,r31,28		/* save r3 on stack */
	bsr.n	_sigsetmask
	ld	r2,r2,4
	ld	r2,r31,24		/* restore r2 from stack */
	ld	r3,r31,28		/* restore r3 from stack */
	ld	r30,r2,72		/* restore r30 */
	ld	r31,r2,76		/* restore r31 */
	ld	r1,r2,0			/* restore r1 */
	jmp.n	r1			/* and jump to it */
	or	r2,r0,r3		/* but first return r3 value */
