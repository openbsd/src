#include "SYS.h"
#include <machine/asm.h>

	.text
ENTRY(cerror)
#if defined(_THREAD_SAFE) || defined (PIC)
	stwu	1, -16(1) 		/* follow the SVR4 ABI */
	mflr	3			/* error value is in r0, use r3 */
	stw	3, 20(1)
#endif
#ifndef  PIC
# ifndef _THREAD_SAFE
	addis 5, 0, errno@H
	ori 5, 5, errno@L
# else
	mr	9, 0
	bl	_C_LABEL(__error)
	mr	5, 3
	mr	0, 9
# endif
#else
# ifndef _THREAD_SAFE
	bl	_GLOBAL_OFFSET_TABLE_@local-4
	mflr	4
	lwz	5,_C_LABEL(errno)@got(4)
# else
	stw	0,  12(1)

	bl	PIC_PLT(_C_LABEL(__error))
	mr	5, 3
	lwz	0,  12(1)
# endif
#endif

	stw 0, 0(5)
	addi 3, 0, -1
	addi 4, 0, -1
#if defined(_THREAD_SAFE) || defined (PIC)
	lwz	0, 20(1)
	mtlr	0
	addi	1, 1, 16	/* follow the SVR4 ABI */
#endif
	blr
