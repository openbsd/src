	.macro	m arg1 arg2 arg3
	.long	\arg1
	.ifc	,\arg2\arg3
	.ELSE
	m	\arg2,\arg3
	.endif
	.endm

	m	foo1,foo2,foo3
