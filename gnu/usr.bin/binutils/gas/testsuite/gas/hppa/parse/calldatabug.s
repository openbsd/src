	.SPACE $PRIVATE$
	.SUBSPA $DATA$,QUAD=1,ALIGN=8,ACCESS=31
	.SPACE $TEXT$
	.SUBSPA $LIT$,QUAD=0,ALIGN=8,ACCESS=44
	.SUBSPA $CODE$,QUAD=0,ALIGN=8,ACCESS=44,CODE_ONLY
	.IMPORT $global$,DATA
	.IMPORT $$dyncall,MILLICODE
; gcc_compiled.:
	.IMPORT printf,CODE
	.SPACE $TEXT$
	.SUBSPA $CODE$

	.align 4
LC$0000:
	.STRING "%d %lf %d\x0a\x00"
	.align 4
	.EXPORT error__3AAAiidi
	.EXPORT error__3AAAiidi,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=FR,ARGW4=FU,RTNVAL=GR
error__3AAAiidi:
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw 2,-20(0,30)
	copy 4,1
	copy 30,4
	stwm 1,128(0,30)
	stw 9,8(0,4)
	stw 8,12(0,4)
	stw 7,16(0,4)
	stw 6,20(0,4)
	stw 5,24(0,4)
	copy %r26,%r5
	ldo -8(0),%r6
	ldo -32(%r4),%r19
	add %r19,%r6,%r7
	stw %r25,0(0,%r7)
	ldo -12(0),%r8
	ldo -32(%r4),%r19
	add %r19,%r8,%r9
	stw %r24,0(0,%r9)
	ldo -8(0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -24(0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldo -28(0),%r21
	ldo -32(%r4),%r22
	add %r22,%r21,%r21
	ldw 0(0,%r21),%r22
	stw %r22,-52(0,%r30)
	ldil L'LC$0000,%r26
	ldo R'LC$0000(%r26),%r26
	ldw 0(0,%r19),%r25
	fldds 0(0,%r20),%fr7
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=FR,ARGW3=FU
	bl printf,2
	nop
	bl,n L$0002,0
	bl,n L$0001,0
L$0002:
L$0001:
	ldw 8(0,4),9
	ldw 12(0,4),8
	ldw 16(0,4),7
	ldw 20(0,4),6
	ldw 24(0,4),5
	ldo 8(4),30
	ldw -28(0,30),2
	bv 0(2)
	ldwm -8(30),4
	.EXIT
	.PROCEND
	.align 4
	.EXPORT ok__3AAAidi
	.EXPORT ok__3AAAidi,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,ARGW2=FR,ARGW3=FU,RTNVAL=GR
ok__3AAAidi:
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw 2,-20(0,30)
	copy 4,1
	copy 30,4
	stwm 1,128(0,30)
	stw 9,8(0,4)
	stw 8,12(0,4)
	stw 7,16(0,4)
	stw 6,20(0,4)
	stw 5,24(0,4)
	copy %r26,%r5
	ldo -8(0),%r6
	ldo -32(%r4),%r19
	add %r19,%r6,%r7
	stw %r25,0(0,%r7)
	ldo -16(0),%r8
	ldo -32(%r4),%r19
	add %r19,%r8,%r9
	fstds %fr7,0(0,%r9)
	ldo -8(0),%r19
	ldo -32(%r4),%r20
	add %r20,%r19,%r19
	ldo -16(0),%r20
	ldo -32(%r4),%r21
	add %r21,%r20,%r20
	ldo -20(0),%r21
	ldo -32(%r4),%r22
	add %r22,%r21,%r21
	ldw 0(0,%r21),%r22
	stw %r22,-52(0,%r30)
	ldil L'LC$0000,%r26
	ldo R'LC$0000(%r26),%r26
	ldw 0(0,%r19),%r25
	fldds 0(0,%r20),%fr7
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=FR,ARGW3=FU
	bl printf,2
	nop
	bl,n L$0004,0
	bl,n L$0003,0
L$0004:
L$0003:
	ldw 8(0,4),9
	ldw 12(0,4),8
	ldw 16(0,4),7
	ldw 20(0,4),6
	ldw 24(0,4),5
	ldo 8(4),30
	ldw -28(0,30),2
	bv 0(2)
	ldwm -8(30),4
	.EXIT
	.PROCEND
	.IMPORT __main,CODE
	.align 8
LC$0001:
	; .double 5.50000000000000000000e+00
	.word 1075183616 ; = 0x40160000
	.word 0 ; = 0x0
	.align 4
	.EXPORT main
	.EXPORT main,PRIV_LEV=3,RTNVAL=GR
main:
	.PROC
	.CALLINFO FRAME=128,CALLS,SAVE_RP
	.ENTRY
	stw 2,-20(0,30)
	copy 4,1
	copy 30,4
	stwm 1,128(0,30)
	.CALL 
	bl __main,2
	nop
	ldo -24(0),%r19
	ldo -32(%r30),%r20
	add %r20,%r19,%r19
	ldil L'LC$0001,%r20
	ldo R'LC$0001(%r20),%r21
	ldw 0(0,%r21),%r22
	ldw 4(0,%r21),%r23
	stw %r22,0(0,%r19)
	stw %r23,4(0,%r19)
	ldo 3(0),%r19
	stw %r19,-60(0,%r30)
	ldo 8(%r4),%r26
	ldo 1(0),%r25
	ldo 4(0),%r24
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl error__3AAAiidi,2
	nop
	ldo 3(0),%r19
	stw %r19,-52(0,%r30)
	ldo 8(%r4),%r26
	ldo 1(0),%r25
	ldil L'LC$0001,%r19
	ldo R'LC$0001(%r19),%r20
	fldds 0(0,%r20),%fr7
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=FR,ARGW3=FU
	bl ok__3AAAidi,2
	nop
	copy 0,%r28
	bl,n L$0005,0
	bl,n L$0005,0
L$0005:
	ldo 8(4),30
	ldw -28(0,30),2
	bv 0(2)
	ldwm -8(30),4
	.EXIT
	.PROCEND

