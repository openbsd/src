#include "SYS.h"

ENTRY(_setjmp)
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
	jmp.n	r1
	or	r2,r0,0

ENTRY(_longjmp)
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
	ld	r30,r2,72
	ld	r31,r2,76
	ld	r1,r2,0
	bcnd.n	ne0,r3,dsrlbl
	or	r2,r3,0
	or	r2,r0,1
     dsrlbl:
	jmp	r1
