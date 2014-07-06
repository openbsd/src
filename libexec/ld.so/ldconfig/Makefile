#	$OpenBSD: Makefile,v 1.11 2014/07/06 17:33:10 otto Exp $
#	$NetBSD: Makefile,v 1.10 1995/03/06 04:24:41 cgd Exp $

MAN=	ldconfig.8

.include <bsd.own.mk>

PROG=	ldconfig
SRCS=	ldconfig.c shlib.c etc.c prebind_delete.c debug.c prebind.c library.c sod.c prebind_path.c
LDDIR?= $(.CURDIR)/..
CFLAGS+=-Wall
CFLAGS+=-I$(.CURDIR) -I$(.CURDIR)/..
LDSTATIC=${STATIC}
BINDIR=	/sbin

#.PATH: $(LDDIR) $(LDDIR)/$(MACHINE_ARCH)

.include <bsd.prog.mk>
