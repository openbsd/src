	.SPACE $PRIVATE$
	.SUBSPA $DATA$,QUAD=1,ALIGN=8,ACCESS=31
	.SUBSPA $BSS$,QUAD=1,ALIGN=8,ACCESS=31,ZERO,SORT=82
	.SPACE $TEXT$
	.SUBSPA $LIT$,QUAD=0,ALIGN=8,ACCESS=44
	.SUBSPA $CODE$,QUAD=0,ALIGN=8,ACCESS=44,CODE_ONLY

	.SPACE $TEXT$
	.SUBSPA $CODE$

	.align 4
; Basic copr memory tests which also test the various 
; addressing modes and completers.
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
; 
copr_indexing_load 

	cldwx,4 5(0,4),26
	cldwx,4,s 5(0,4),26
	cldwx,4,m 5(0,4),26
	cldwx,4,sm 5(0,4),26
	clddx,4 5(0,4),26
	clddx,4,s 5(0,4),26
	clddx,4,m 5(0,4),26
	clddx,4,sm 5(0,4),26

copr_indexing_store 
	cstwx,4 26,5(0,4)
	cstwx,4,s 26,5(0,4)
	cstwx,4,m 26,5(0,4)
	cstwx,4,sm 26,5(0,4)
	cstdx,4 26,5(0,4)
	cstdx,4,s 26,5(0,4)
	cstdx,4,m 26,5(0,4)
	cstdx,4,sm 26,5(0,4)

copr_short_memory 
	cldws,4 0(0,4),26
	cldws,4,mb 0(0,4),26
	cldws,4,ma 0(0,4),26
	cldds,4 0(0,4),26
	cldds,4,mb 0(0,4),26
	cldds,4,ma 0(0,4),26
	cstws,4 26,0(0,4)
	cstws,4,mb 26,0(0,4)
	cstws,4,ma 26,0(0,4)
	cstds,4 26,0(0,4)
	cstds,4,mb 26,0(0,4)
	cstds,4,ma 26,0(0,4)

; gas fucks this up thinks it gets the expression 4 modulo 5
;	cldwx,4 %r5(0,%r4),%r26
