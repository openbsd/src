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
	or %r4,%r5,%r6
	or,= %r4,%r5,%r6
	or,< %r4,%r5,%r6
	or,<= %r4,%r5,%r6
	or,od %r4,%r5,%r6
	or,tr %r4,%r5,%r6
	or,<> %r4,%r5,%r6
	or,>= %r4,%r5,%r6
	or,> %r4,%r5,%r6
	or,ev %r4,%r5,%r6

	xor %r4,%r5,%r6
	xor,= %r4,%r5,%r6
	xor,< %r4,%r5,%r6
	xor,<= %r4,%r5,%r6
	xor,od %r4,%r5,%r6
	xor,tr %r4,%r5,%r6
	xor,<> %r4,%r5,%r6
	xor,>= %r4,%r5,%r6
	xor,> %r4,%r5,%r6
	xor,ev %r4,%r5,%r6

	and %r4,%r5,%r6
	and,= %r4,%r5,%r6
	and,< %r4,%r5,%r6
	and,<= %r4,%r5,%r6
	and,od %r4,%r5,%r6
	and,tr %r4,%r5,%r6
	and,<> %r4,%r5,%r6
	and,>= %r4,%r5,%r6
	and,> %r4,%r5,%r6
	and,ev %r4,%r5,%r6

	andcm %r4,%r5,%r6
	andcm,= %r4,%r5,%r6
	andcm,< %r4,%r5,%r6
	andcm,<= %r4,%r5,%r6
	andcm,od %r4,%r5,%r6
	andcm,tr %r4,%r5,%r6
	andcm,<> %r4,%r5,%r6
	andcm,>= %r4,%r5,%r6
	andcm,> %r4,%r5,%r6
	andcm,ev %r4,%r5,%r6

