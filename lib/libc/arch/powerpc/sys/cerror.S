#include "SYS.h"
#include <machine/asm.h>

	.text
ENTRY(cerror)
#if 1
	addis 5, 0, errno@H
	ori 5, 5, errno@L
#else
	.data
	.extern errno
errno_toc:
	.long errno
	.text
	addis 5, 0, errno_toc@H
	ori 5, 5, errno_toc@L
	lwz 5, 0(5)
#endif
	stw 0, 0(5)
	addi 3, 0, -1
	addi 4, 0, -1
	blr
