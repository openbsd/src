#include "SYS.h"
#include <machine/asm.h>

	.text
ENTRY(cerror)
#if defined(_THREAD_SAFE) || defined (PIC)
	stwu	1, -16(1) 		/* follow the SVR4 ABI */
#endif
#ifndef  PIC
# ifndef _THREAD_SAFE
	addis 5, 0, errno@H
	ori 5, 5, errno@L
# else
	mr	9, 0
	mflr	10
	bl	_C_LABEL(__error)
	mr	3, 5
	mtlr	10
	mr	0, 9
# endif
#else
# ifndef _THREAD_SAFE
	mflr	10
	bl	_GLOBAL_OFFSET_TABLE_@local-4
	mflr	4
	mtlr	10
	lwz	5,_C_LABEL(errno)@got(4)
# else
	mflr	10
	stw	10, 20(1)
	stw	0,  12(1)

	bl	PIC_PLT(_C_LABEL(__error))
	mr	5, 3
	lwz	10, 12(1)
	mtlr	10
	lwz	0,  20(1)
# endif
#endif

	stw 0, 0(5)
	addi 3, 0, -1
	addi 4, 0, -1
#if defined(_THREAD_SAFE) || defined (PIC)
	addi	1, 1, 16	/* follow the SVR4 ABI */
#endif
	blr
