	.SPACE $PRIVATE$
	.SUBSPA $DATA$,QUAD=1,ALIGN=8,ACCESS=31
	.SUBSPA $BSS$,QUAD=1,ALIGN=8,ACCESS=31,ZERO,SORT=82
	.SPACE $TEXT$
	.SUBSPA $LIT$,QUAD=0,ALIGN=8,ACCESS=44
	.SUBSPA $CODE$,QUAD=0,ALIGN=8,ACCESS=44,CODE_ONLY
	.IMPORT $global$,DATA
	.IMPORT $$dyncall,MILLICODE
; gcc_compiled.:
	.SPACE $TEXT$
	.SUBSPA $CODE$

	.align 4
	.EXPORT foobar,ENTRY,PRIV_LEV=3,ARGW0=FR,ARGW1=FU,ARGW2=FR,ARGW3=FU,RTNVAL=FR
foobar
	.PROC
	.CALLINFO FRAME=0,NO_CALLS
	.ENTRY
	ldo -64(%r30),%r20
	addil LR'x-$global$,%r27
	fldds 8(0,%r20),%fr4
	fldds 0(0,%r20),%fr22
	ldo RR'x-$global$(%r1),%r19
	fmpysub,sgl %fr5L,%fr7L,%fr5L,%fr22L,%fr4L
	bv 0(%r2)
	fstds %fr5,0(0,%r19)
	.EXIT
	.PROCEND
	.SPACE $PRIVATE$
	.SUBSPA $BSS$

x	.comm 8
y	.comm 8
