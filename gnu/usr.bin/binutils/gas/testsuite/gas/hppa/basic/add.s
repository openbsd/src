	.SPACE $PRIVATE$
	.SUBSPA $DATA$,QUAD=1,ALIGN=8,ACCESS=31
	.SUBSPA $BSS$,QUAD=1,ALIGN=8,ACCESS=31,ZERO,SORT=82
	.SPACE $TEXT$
	.SUBSPA $LIT$,QUAD=0,ALIGN=8,ACCESS=44
	.SUBSPA $CODE$,QUAD=0,ALIGN=8,ACCESS=44,CODE_ONLY

	.SPACE $TEXT$
	.SUBSPA $CODE$

	.align 4
; Basic add/sh?add instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	add  %r4,%r5,%r6
	add,=  %r4,%r5,%r6
	add,<  %r4,%r5,%r6
	add,<=  %r4,%r5,%r6
	add,nuv  %r4,%r5,%r6
	add,znv  %r4,%r5,%r6
	add,sv  %r4,%r5,%r6
	add,od  %r4,%r5,%r6
	add,tr  %r4,%r5,%r6
	add,<>  %r4,%r5,%r6
	add,>=  %r4,%r5,%r6
	add,>  %r4,%r5,%r6
	add,uv  %r4,%r5,%r6
	add,vnz  %r4,%r5,%r6
	add,nsv  %r4,%r5,%r6
	add,ev  %r4,%r5,%r6

	addl  %r4,%r5,%r6
	addl,=  %r4,%r5,%r6
	addl,<  %r4,%r5,%r6
	addl,<=  %r4,%r5,%r6
	addl,nuv  %r4,%r5,%r6
	addl,znv  %r4,%r5,%r6
	addl,sv  %r4,%r5,%r6
	addl,od  %r4,%r5,%r6
	addl,tr  %r4,%r5,%r6
	addl,<>  %r4,%r5,%r6
	addl,>=  %r4,%r5,%r6
	addl,>  %r4,%r5,%r6
	addl,uv  %r4,%r5,%r6
	addl,vnz  %r4,%r5,%r6
	addl,nsv  %r4,%r5,%r6
	addl,ev  %r4,%r5,%r6

	addo  %r4,%r5,%r6
	addo,=  %r4,%r5,%r6
	addo,<  %r4,%r5,%r6
	addo,<=  %r4,%r5,%r6
	addo,nuv  %r4,%r5,%r6
	addo,znv  %r4,%r5,%r6
	addo,sv  %r4,%r5,%r6
	addo,od  %r4,%r5,%r6
	addo,tr  %r4,%r5,%r6
	addo,<>  %r4,%r5,%r6
	addo,>=  %r4,%r5,%r6
	addo,>  %r4,%r5,%r6
	addo,uv  %r4,%r5,%r6
	addo,vnz  %r4,%r5,%r6
	addo,nsv  %r4,%r5,%r6
	addo,ev  %r4,%r5,%r6

	addc  %r4,%r5,%r6
	addc,=  %r4,%r5,%r6
	addc,<  %r4,%r5,%r6
	addc,<=  %r4,%r5,%r6
	addc,nuv  %r4,%r5,%r6
	addc,znv  %r4,%r5,%r6
	addc,sv  %r4,%r5,%r6
	addc,od  %r4,%r5,%r6
	addc,tr  %r4,%r5,%r6
	addc,<>  %r4,%r5,%r6
	addc,>=  %r4,%r5,%r6
	addc,>  %r4,%r5,%r6
	addc,uv  %r4,%r5,%r6
	addc,vnz  %r4,%r5,%r6
	addc,nsv  %r4,%r5,%r6
	addc,ev  %r4,%r5,%r6

	addco  %r4,%r5,%r6
	addco,=  %r4,%r5,%r6
	addco,<  %r4,%r5,%r6
	addco,<=  %r4,%r5,%r6
	addco,nuv  %r4,%r5,%r6
	addco,znv  %r4,%r5,%r6
	addco,sv  %r4,%r5,%r6
	addco,od  %r4,%r5,%r6
	addco,tr  %r4,%r5,%r6
	addco,<>  %r4,%r5,%r6
	addco,>=  %r4,%r5,%r6
	addco,>  %r4,%r5,%r6
	addco,uv  %r4,%r5,%r6
	addco,vnz  %r4,%r5,%r6
	addco,nsv  %r4,%r5,%r6
	addco,ev  %r4,%r5,%r6
