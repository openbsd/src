#include "SYS.h"
#include <machine/asm.h>

	.text
ASENTRY(__cerror)
#if defined (PIC)
	stwu	1, -16(1) 		/* follow the SVR4 ABI */
	mflr	3			/* error value is in r0, use r3 */
	stw	3, 20(1)
	bl	_GLOBAL_OFFSET_TABLE_@local-4
	mflr	4
	lwz	5,_C_LABEL(errno)@got(4)
#else
	addis 5, 0, errno@H
	ori 5, 5, errno@L
#endif

	stw 0, 0(5)
	addi 3, 0, -1
	addi 4, 0, -1
#if defined (PIC)
	lwz	0, 20(1)
	mtlr	0
	addi	1, 1, 16	/* follow the SVR4 ABI */
#endif
	blr
