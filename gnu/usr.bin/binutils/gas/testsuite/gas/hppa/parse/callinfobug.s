	.space $TEXT$
	.subspa $CODE$
	.align 4
	.export divu,millicode
	.proc
	.callinfo millicode
divu
	.procend
