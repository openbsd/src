	.SPACE $PRIVATE$
	.SUBSPA $DATA$,QUAD=1,ALIGN=8,ACCESS=31
	.SUBSPA $BSS$,QUAD=1,ALIGN=8,ACCESS=31,ZERO,SORT=82
	.SPACE $TEXT$
	.SUBSPA $LIT$,QUAD=0,ALIGN=8,ACCESS=44
	.SUBSPA $CODE$,QUAD=0,ALIGN=8,ACCESS=44,CODE_ONLY

	.SPACE $TEXT$
	.SUBSPA $CODE$

	.align 4
	.IMPORT foo,data

; Switch in/out of different rounding modes.
; Also make sure we "optimize" away useless rounding mode relocations
	addil   LR'foo-0x12345,%r27
	ldo	RR'foo-0x12345(%r1),%r1
	addil   L'foo-0x12345,%r27
	ldo	R'foo-0x12345(%r1),%r1
	addil   LR'foo-0x12345,%r27
	ldo	RR'foo-0x12345(%r1),%r1
	addil   LR'foo-0x12345,%r27
	ldo	RR'foo-0x12345(%r1),%r1
