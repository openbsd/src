#include "SYS.h"

ENTRY(brk)
	bv	r0(rp)
	nop
EXIT(brk)

	.end
