	.SPACE $PRIVATE$
	.SUBSPA $DATA$,QUAD=1,ALIGN=8,ACCESS=31
	.SUBSPA $BSS$,QUAD=1,ALIGN=8,ACCESS=31,ZERO,SORT=82
	.SPACE $TEXT$
	.SUBSPA $LIT$,QUAD=0,ALIGN=8,ACCESS=44
	.SUBSPA $CODE$,QUAD=0,ALIGN=8,ACCESS=44,CODE_ONLY

	.SPACE $PRIVATE$
	.SUBSPA $DATA$


; GAS used to mis-parse the embedded quotes
	.STRING "#include \"awk.def\"\x0a\x00"

; Octal escapes used to consume > 3 chars which led to this
; string being screwed in a big way.
	.STRING "\0110x123"


