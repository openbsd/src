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
	addi  123,%r5,%r6
	addi,=  123,%r5,%r6
	addi,<  123,%r5,%r6
	addi,<=  123,%r5,%r6
	addi,nuv  123,%r5,%r6
	addi,znv  123,%r5,%r6
	addi,sv  123,%r5,%r6
	addi,od  123,%r5,%r6
	addi,tr  123,%r5,%r6
	addi,<>  123,%r5,%r6
	addi,>=  123,%r5,%r6
	addi,>  123,%r5,%r6
	addi,uv  123,%r5,%r6
	addi,vnz  123,%r5,%r6
	addi,nsv  123,%r5,%r6
	addi,ev  123,%r5,%r6

	addio  123,%r5,%r6
	addio,=  123,%r5,%r6
	addio,<  123,%r5,%r6
	addio,<=  123,%r5,%r6
	addio,nuv  123,%r5,%r6
	addio,znv  123,%r5,%r6
	addio,sv  123,%r5,%r6
	addio,od  123,%r5,%r6
	addio,tr  123,%r5,%r6
	addio,<>  123,%r5,%r6
	addio,>=  123,%r5,%r6
	addio,>  123,%r5,%r6
	addio,uv  123,%r5,%r6
	addio,vnz  123,%r5,%r6
	addio,nsv  123,%r5,%r6
	addio,ev  123,%r5,%r6

	addit  123,%r5,%r6
	addit,=  123,%r5,%r6
	addit,<  123,%r5,%r6
	addit,<=  123,%r5,%r6
	addit,nuv  123,%r5,%r6
	addit,znv  123,%r5,%r6
	addit,sv  123,%r5,%r6
	addit,od  123,%r5,%r6
	addit,tr  123,%r5,%r6
	addit,<>  123,%r5,%r6
	addit,>=  123,%r5,%r6
	addit,>  123,%r5,%r6
	addit,uv  123,%r5,%r6
	addit,vnz  123,%r5,%r6
	addit,nsv  123,%r5,%r6
	addit,ev  123,%r5,%r6

	addito  123,%r5,%r6
	addito,=  123,%r5,%r6
	addito,<  123,%r5,%r6
	addito,<=  123,%r5,%r6
	addito,nuv  123,%r5,%r6
	addito,znv  123,%r5,%r6
	addito,sv  123,%r5,%r6
	addito,od  123,%r5,%r6
	addito,tr  123,%r5,%r6
	addito,<>  123,%r5,%r6
	addito,>=  123,%r5,%r6
	addito,>  123,%r5,%r6
	addito,uv  123,%r5,%r6
	addito,vnz  123,%r5,%r6
	addito,nsv  123,%r5,%r6
	addito,ev  123,%r5,%r6
