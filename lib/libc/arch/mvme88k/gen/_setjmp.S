#include "SYS.h"

ENTRY(_setjmp)
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
	jmp.n	r1
	or	r2,r0,0

ENTRY(_longjmp)
	ld.d	r14,r2,8
	ld.d	r16,r2,16
	ld.d	r18,r2,24
	ld.d	r20,r2,32
	ld.d	r22,r2,40
	ld.d	r24,r2,48
	ld.d	r26,r2,56
	ld.d	r28,r2,64
	ld.d	r30,r2,72
	ld	r1,r2,0
	bcnd.n	ne0,r3,dsrlbl
	or	r2,r3,0
	or	r2,r0,1
     dsrlbl:
	jmp	r1
