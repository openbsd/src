#	$OpenBSD: Makefile,v 1.7 2006/05/12 23:20:52 deraadt Exp $
#	$NetBSD: Makefile,v 1.10 1995/03/06 04:24:41 cgd Exp $

PROG=	ldconfig
SRCS=	ldconfig.c shlib.c etc.c prebind_delete.c debug.c prebind.c library.c sod.c
LDDIR?= $(.CURDIR)/..
#CFLAGS+=-Werror
CFLAGS+=-I$(.CURDIR) -I$(.CURDIR)/..
LDSTATIC=${STATIC}
BINDIR=	/sbin
MAN=	ldconfig.8

#.PATH: $(LDDIR) $(LDDIR)/$(MACHINE_ARCH)

.include <bsd.prog.mk>
