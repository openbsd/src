#	$OpenBSD: Makefile,v 1.8 2013/02/17 22:06:31 deraadt Exp $
#	$NetBSD: Makefile,v 1.10 1995/03/06 04:24:41 cgd Exp $

MAN=	ldconfig.8

.include <bsd.own.mk>

.if (${ELF_TOOLCHAIN:L} == "yes")

PROG=	ldconfig
SRCS=	ldconfig.c shlib.c etc.c prebind_delete.c debug.c prebind.c library.c sod.c
LDDIR?= $(.CURDIR)/..
#CFLAGS+=-Werror
CFLAGS+=-I$(.CURDIR) -I$(.CURDIR)/..
LDSTATIC=${STATIC}
BINDIR=	/sbin

#.PATH: $(LDDIR) $(LDDIR)/$(MACHINE_ARCH)

.endif

.include <bsd.prog.mk>
