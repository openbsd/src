#	$OpenBSD: Makefile,v 1.3 2000/01/01 10:02:07 matthieu Exp $
#	$NetBSD: Makefile,v 1.1 1996/02/21 02:45:47 jtk Exp $

.PATH: ${LIBC}/i386
NOPIC=

SRCS+=	i386_get_ldt.c i386_set_ldt.c \
	i386_iopl.c i386_get_ioperm.c i386_set_ioperm.c \
	i386_vm86.c

MAN+=	i386_get_ldt.2 \
	i386_iopl.2 i386_get_ioperm.2 \
	i386_vm86.2

MANSUBDIR=/i386
MLINKS+=i386_get_ldt.2 i386_set_ldt.2 \
	i386_get_ioperm.2 i386_set_ioperm.2

.include <bsd.lib.mk>

