#	$OpenBSD: Makefile,v 1.6 2011/03/12 04:12:29 guenther Exp $
#	$NetBSD: Makefile,v 1.1 1996/02/21 02:45:47 jtk Exp $

MAN+=	i386_get_ldt.2 \
	i386_get_fsbase.2 i386_get_gsbase.2 \
	i386_iopl.2 i386_get_ioperm.2 \
	i386_vm86.2
MANSUBDIR=i386
MLINKS+=i386_get_ldt.2 i386_set_ldt.2 \
	i386_get_fsbase.2 i386_set_fsbase.2 \
	i386_get_gsbase.2 i386_set_gsbase.2 \
	i386_get_ioperm.2 i386_set_ioperm.2

.if ${MACHINE} == "i386"
.PATH: ${LIBC}/i386
NOPIC=
SRCS+=	i386_get_ldt.c i386_set_ldt.c \
	i386_iopl.c i386_get_ioperm.c i386_set_ioperm.c \
	i386_get_fsbase.c i386_get_gsbase.c \
	i386_set_fsbase.c i386_set_gsbase.c \
	i386_vm86.c
.include <bsd.lib.mk>
.else
NOPROG=
.include <bsd.prog.mk>
.endif

