#include "SYS.h"
	.extern _C_LABEL(_logname_valid)
RSYSCALL(setlogin)
	li	4, 4
#ifndef PIC
	lis	0,_C_LABEL(_logname_valid)@H
	stw	4,_C_LABEL(_logname_valid)@L(0)
#else
	mflr	10
	bl	_GLOBAL_OFFSET_TABLE_@local-4
	mflr	9
	mtlr	10
	lwz	5,_C_LABEL(_logname_valid)@got(9)
	stw	4,0(5)
#endif
	blr
