/* int ptrace(int *mask) */

#include "SYS.h"

	.text
PREFIX(ptrace)
	sc
	cmpwi 0, 0
 	beq ok
	b	PIC_PLT(_ASM_LABEL(cerror))

ok:
	blr
