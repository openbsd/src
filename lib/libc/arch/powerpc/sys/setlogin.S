#include "SYS.h"
	.extern __logname_valid
RSYSCALL(setlogin)
	lis	0, __logname_valid@H
	li	4, 4
	stw	4,__logname_valid@L(0)
	blr

