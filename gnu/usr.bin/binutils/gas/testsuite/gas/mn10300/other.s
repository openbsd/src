	.text
	clr d2
	inc d1
	inc a2
	inc4 a3
	jmp a2
	jmp 256
	jmp 131071
	call 256,5,9
	call 131071,9,32
	calls a2
	calls 256
	calls 131071
	ret 15,7
	retf 9,5
	rets
	rti
	trap
	nop
	rtm
