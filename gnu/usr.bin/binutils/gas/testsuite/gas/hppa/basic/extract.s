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
	extru %r4,5,10,%r6
	extru,= %r4,5,10,%r6
	extru,< %r4,5,10,%r6
	extru,od %r4,5,10,%r6
	extru,tr %r4,5,10,%r6
	extru,<> %r4,5,10,%r6
	extru,>= %r4,5,10,%r6
	extru,ev %r4,5,10,%r6

	extrs %r4,5,10,%r6
	extrs,= %r4,5,10,%r6
	extrs,< %r4,5,10,%r6
	extrs,od %r4,5,10,%r6
	extrs,tr %r4,5,10,%r6
	extrs,<> %r4,5,10,%r6
	extrs,>= %r4,5,10,%r6
	extrs,ev %r4,5,10,%r6

	vextru %r4,5,%r6
	vextru,= %r4,5,%r6
	vextru,< %r4,5,%r6
	vextru,od %r4,5,%r6
	vextru,tr %r4,5,%r6
	vextru,<> %r4,5,%r6
	vextru,>= %r4,5,%r6
	vextru,ev %r4,5,%r6

	vextrs %r4,5,%r6
	vextrs,= %r4,5,%r6
	vextrs,< %r4,5,%r6
	vextrs,od %r4,5,%r6
	vextrs,tr %r4,5,%r6
	vextrs,<> %r4,5,%r6
	vextrs,>= %r4,5,%r6
	vextrs,ev %r4,5,%r6
