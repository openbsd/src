	.SPACE $PRIVATE$
	.SUBSPA $DATA$,QUAD=1,ALIGN=8,ACCESS=31
	.SUBSPA $BSS$,QUAD=1,ALIGN=8,ACCESS=31,ZERO,SORT=82
	.SPACE $TEXT$
	.SUBSPA $LIT$,QUAD=0,ALIGN=8,ACCESS=44
	.SUBSPA $CODE$,QUAD=0,ALIGN=8,ACCESS=44,CODE_ONLY

	.SPACE $TEXT$
	.SUBSPA $CODE$

	.align 4
; Basic immediate instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	vshd %r4,%r5,%r6
	vshd,= %r4,%r5,%r6
	vshd,< %r4,%r5,%r6
	vshd,od %r4,%r5,%r6
	vshd,tr %r4,%r5,%r6
	vshd,<> %r4,%r5,%r6
	vshd,>= %r4,%r5,%r6
	vshd,ev %r4,%r5,%r6

	shd %r4,%r5,5,%r6
	shd,= %r4,%r5,5,%r6
	shd,< %r4,%r5,5,%r6
	shd,od %r4,%r5,5,%r6
	shd,tr %r4,%r5,5,%r6
	shd,<> %r4,%r5,5,%r6
	shd,>= %r4,%r5,5,%r6
	shd,ev %r4,%r5,5,%r6

