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
	sub %r4,%r5,%r6
	sub,= %r4,%r5,%r6
	sub,< %r4,%r5,%r6
	sub,<= %r4,%r5,%r6
	sub,<< %r4,%r5,%r6
	sub,<<= %r4,%r5,%r6
	sub,sv %r4,%r5,%r6
	sub,od %r4,%r5,%r6
	sub,tr %r4,%r5,%r6
	sub,<> %r4,%r5,%r6
	sub,>= %r4,%r5,%r6
	sub,> %r4,%r5,%r6
	sub,>>= %r4,%r5,%r6
	sub,>> %r4,%r5,%r6
	sub,nsv %r4,%r5,%r6
	sub,ev %r4,%r5,%r6

	subo %r4,%r5,%r6
	subo,= %r4,%r5,%r6
	subo,< %r4,%r5,%r6
	subo,<= %r4,%r5,%r6
	subo,<< %r4,%r5,%r6
	subo,<<= %r4,%r5,%r6
	subo,sv %r4,%r5,%r6
	subo,od %r4,%r5,%r6
	subo,tr %r4,%r5,%r6
	subo,<> %r4,%r5,%r6
	subo,>= %r4,%r5,%r6
	subo,> %r4,%r5,%r6
	subo,>>= %r4,%r5,%r6
	subo,>> %r4,%r5,%r6
	subo,nsv %r4,%r5,%r6
	subo,ev %r4,%r5,%r6

	subb %r4,%r5,%r6
	subb,= %r4,%r5,%r6
	subb,< %r4,%r5,%r6
	subb,<= %r4,%r5,%r6
	subb,<< %r4,%r5,%r6
	subb,<<= %r4,%r5,%r6
	subb,sv %r4,%r5,%r6
	subb,od %r4,%r5,%r6
	subb,tr %r4,%r5,%r6
	subb,<> %r4,%r5,%r6
	subb,>= %r4,%r5,%r6
	subb,> %r4,%r5,%r6
	subb,>>= %r4,%r5,%r6
	subb,>> %r4,%r5,%r6
	subb,nsv %r4,%r5,%r6
	subb,ev %r4,%r5,%r6

	subbo %r4,%r5,%r6
	subbo,= %r4,%r5,%r6
	subbo,< %r4,%r5,%r6
	subbo,<= %r4,%r5,%r6
	subbo,<< %r4,%r5,%r6
	subbo,<<= %r4,%r5,%r6
	subbo,sv %r4,%r5,%r6
	subbo,od %r4,%r5,%r6
	subbo,tr %r4,%r5,%r6
	subbo,<> %r4,%r5,%r6
	subbo,>= %r4,%r5,%r6
	subbo,> %r4,%r5,%r6
	subbo,>>= %r4,%r5,%r6
	subbo,>> %r4,%r5,%r6
	subbo,nsv %r4,%r5,%r6
	subbo,ev %r4,%r5,%r6

	subt %r4,%r5,%r6
	subt,= %r4,%r5,%r6
	subt,< %r4,%r5,%r6
	subt,<= %r4,%r5,%r6
	subt,<< %r4,%r5,%r6
	subt,<<= %r4,%r5,%r6
	subt,sv %r4,%r5,%r6
	subt,od %r4,%r5,%r6
	subt,tr %r4,%r5,%r6
	subt,<> %r4,%r5,%r6
	subt,>= %r4,%r5,%r6
	subt,> %r4,%r5,%r6
	subt,>>= %r4,%r5,%r6
	subt,>> %r4,%r5,%r6
	subt,nsv %r4,%r5,%r6
	subt,ev %r4,%r5,%r6

	subto %r4,%r5,%r6
	subto,= %r4,%r5,%r6
	subto,< %r4,%r5,%r6
	subto,<= %r4,%r5,%r6
	subto,<< %r4,%r5,%r6
	subto,<<= %r4,%r5,%r6
	subto,sv %r4,%r5,%r6
	subto,od %r4,%r5,%r6
	subto,tr %r4,%r5,%r6
	subto,<> %r4,%r5,%r6
	subto,>= %r4,%r5,%r6
	subto,> %r4,%r5,%r6
	subto,>>= %r4,%r5,%r6
	subto,>> %r4,%r5,%r6
	subto,nsv %r4,%r5,%r6
	subto,ev %r4,%r5,%r6
