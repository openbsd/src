#	$OpenBSD: Makefile,v 1.10 2016/03/30 06:38:41 jmc Exp $
#	$NetBSD: Makefile,v 1.1 1996/02/21 02:45:47 jtk Exp $

MAN+=	i386_get_fsbase.2 i386_get_gsbase.2 \
	i386_iopl.2 i386_vm86.2
MANSUBDIR=i386

.if ${MACHINE} == "i386"
.PATH: ${LIBC}/i386
NOPIC=
SRCS+=	i386_iopl.c \
	i386_get_fsbase.c i386_get_gsbase.c \
	i386_set_fsbase.c i386_set_gsbase.c \
	i386_vm86.c
.include <bsd.lib.mk>
.else
NOPROG=
.include <bsd.prog.mk>
.endif
