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
	dcor %r4,%r5
	dcor,sbz %r4,%r5
	dcor,shz %r4,%r5
	dcor,sdc %r4,%r5
	dcor,sbc %r4,%r5
	dcor,shc %r4,%r5
	dcor,tr %r4,%r5
	dcor,nbz %r4,%r5
	dcor,nhz %r4,%r5
	dcor,ndc %r4,%r5
	dcor,nbc %r4,%r5
	dcor,nhc %r4,%r5

	idcor %r4,%r5
	idcor,sbz %r4,%r5
	idcor,shz %r4,%r5
	idcor,sdc %r4,%r5
	idcor,sbc %r4,%r5
	idcor,shc %r4,%r5
	idcor,tr %r4,%r5
	idcor,nbz %r4,%r5
	idcor,nhz %r4,%r5
	idcor,ndc %r4,%r5
	idcor,nbc %r4,%r5
	idcor,nhc %r4,%r5
