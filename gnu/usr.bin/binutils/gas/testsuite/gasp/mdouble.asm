
	.MACRO	HI
	A
	\! this is hidden
	B
	! this is not
	C
	.ENDM
	Hello 
	HI
	Emily
	

	H'0f
	200+H'0F

XX	.ASSIGNA	Q'100
! Definition:
	.MACRO	GET X=100,Y,Z
	MOV	#\X+H'0F,@B
	\Y
\Z	JMP	@MAIN
L\@	ADD 	#1,@HL
	MOV	#0,@C	\! Clear C
	ADD	#2,@C
	ADD	#\&XX, @C
	.ENDM

	NOP

!Call:	
	GET	200,"ADD #1,@B", ENTRY
	.END

	; Definition:


	NOP

	;Call:	
	MOV	#200+0F,@B
	ADD #1,@B
ENTRY:	JMP	@MAIN
L00000:	ADD 	#1,@HL
	MOV	#0,@C	
	ADD	#2,@C
	ADD	#0, @C
