get	macro	arg1,arg2,arg3
	dc.l	arg1
	arg2
arg3	dc.l	\4
	move.\0	d0,d1
	endm

	get.b	1,<dc.l 2>,label,four
