#include "SYS.h"

ENTRY(setjmp)
	st	r1, r2,0
	st.d	r14,r2,8
	st.d	r16,r2,16
	st.d	r18,r2,24
	st.d	r20,r2,32
	st.d	r22,r2,40
	st.d	r24,r2,48
	st.d	r26,r2,56
	st.d	r28,r2,64
	st.d	r30,r2,72
	or	r15,r1,0	/* can use r15 */
	or	r14,r2,0	/* can use r14 as it is already saved */
	bsr.n	_sigblock
	or	r2,r0,0
	st	r2,r14,4
	jmp.n	r15
	or	r2,r0,0

ENTRY(longjmp)
	subu	r31,r31,32		/* get some temporary stack */
	ld.d	r14,r2,8
	ld.d	r16,r2,16
	ld.d	r18,r2,24
	ld.d	r20,r2,32
	ld.d	r22,r2,40
	ld.d	r24,r2,48
	ld.d	r26,r2,56
	ld.d	r28,r2,64
	st.d	r2,r31,24		/* save r2 and r3 on stack */
	bsr.n	_sigsetmask
	ld	r2,r2,4
	ld.d	r2,r31,24		/* restore r2 and r3 from stack */
	ld.d	r30,r2,72		/* restore r30 and r31 */
	ld	r1,r2,0			/* restore r1 */
	jmp.n	r1			/* and jump to it */
	or	r2,r0,r3		/* but first return r3 value */
