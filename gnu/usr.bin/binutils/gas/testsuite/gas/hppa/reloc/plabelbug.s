	.SPACE $PRIVATE$
	.SUBSPA $DATA$,QUAD=1,ALIGN=8,ACCESS=31
	.SUBSPA $BSS$,QUAD=1,ALIGN=8,ACCESS=31,ZERO,SORT=82
	.SPACE $TEXT$
	.SUBSPA $LIT$,QUAD=0,ALIGN=8,ACCESS=44
	.SUBSPA $CODE$,QUAD=0,ALIGN=8,ACCESS=44,CODE_ONLY
	.IMPORT $global$,DATA
	.IMPORT $$dyncall,MILLICODE
; gcc_compiled.:
	.IMPORT abort,CODE
	.EXPORT f,DATA
	.SPACE $PRIVATE$
	.SUBSPA $DATA$

	.align 4
f:
	.word P%abort
	.word P%abort
	.IMPORT __main,CODE
	.IMPORT printf,CODE
	.SPACE $TEXT$
	.SUBSPA $CODE$

	.align 4
LC$0000:
	.STRING "frob\x0a\x00"
	.align 4
	.EXPORT main,CODE
	.EXPORT main,ENTRY,PRIV_LEV=3,RTNVAL=GR
main:
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw %r2,-20(0,%r30)
	ldo 128(%r30),%r30
	.CALL 
	bl __main,2
	nop
	ldil L'LC$0000,%r26
	.CALL ARGW0=GR
	bl printf,2
	ldo R'LC$0000(%r26),%r26
	ldw -148(0,%r30),%r2
	bv 0(%r2)
	ldo -128(%r30),%r30
	.EXIT
	.PROCEND
