	.MACRO	SUM FROM=0, TO=9
	; \FROM \TO
	MOV	R\FROM,R10
COUNT	.ASSIGNA	\FROM+1
	.AWHILE	\&COUNT LE \TO
	MOV	R\&COUNT,R10
COUNT	.ASSIGNA	\&COUNT+1
	.AENDW
	.ENDM

	SUM 0,5
	SUM 	TO=5
	SUM 	FROM=2, TO=5


; hi this is a comment
	.MACRO	BACK_SLASH_SET
	\(MOV	#"\",R0) 
	.ENDM
	BACK_SLASH_SET
	.MACRO	COMM
	bar	; this comment will get copied out
	foo	\; this one will get dropped
	.ENDM
	COMM
	BACK_SLASH_SET
	.MACRO	PLUS2
	ADD	#1,R\&V1
	.SDATA	"\&V'1"
	.ENDM
V	.ASSIGNC	"R"
V1	.ASSIGNA	1
	PLUS2
	.MACRO	PLUS1	P,P1
	ADD	#1,\P1
	.SDATA	"\P'1"
	.ENDM
	PLUS1	R,R1

	.MACRO	SUM P1
	MOV	R0,R10
	ADD	R1,R10
	ADD	R2,R10
	\P1	
	ADD	R3,R10
	.ENDM

	SUM	.EXITM

	.MACRO foo bar=a default=b
	\bar
	\default
	bar
	default
	.ENDM
	foo default=dog bar=cat
	foo X Y
	foo
	foo bar=cat default=dog


	.MACRO	foo bar
	HI
	HI \bar
	HI
	.ENDM

	foo 1
	foo 123
	foo 1 2 3 4
	foo

	
	.MACRO	PUSH Rn
	MOV.L	\Rn,@-r15
	.ENDM
	PUSH	R0
	PUSH	R1


	.MACRO	RES_STR STR, Rn
	MOV.L	#str\@,\Rn
	BRA	end_str\@
	NOP
str\@	.SDATA "\STR"
	.ALIGN	2
end_str\@
	.ENDM
	
	RES_STR	"ONE",R0	
	RES_STR	"TWO",R1
	RES_STR	"THREE",R2



	RES_STR STR=donkey Rn=R1
	RES_STR donkey,R1
 	RES_STR donkey Rn=R1
	.END



