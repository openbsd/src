/*	$OpenBSD: os-note-elf.h,v 1.2 2001/02/03 23:00:38 art Exp $	*/
/*
 * Contents:
 *
 *  long Name length
 *  long Description length
 *  long ELF_NOTE_TYPE_OSVERSION (1) XXX - need a define.
 *  "OpenBSD\0"
 *  version? 0 XXX
 */

__asm("	.section \".note.openbsd.ident\", \"a\"
	.p2align 2

	.long	8
	.long	4
	.long	1
	.ascii \"OpenBSD\\0\"
	.long	0

	.p2align 2");