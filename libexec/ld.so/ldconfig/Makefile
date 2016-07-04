#	$OpenBSD: Makefile,v 1.12 2016/07/04 20:56:50 kettenis Exp $
#	$NetBSD: Makefile,v 1.10 1995/03/06 04:24:41 cgd Exp $

MAN=	ldconfig.8

.include <bsd.own.mk>

PROG=	ldconfig
SRCS=	ldconfig.c shlib.c etc.c sod.c ldconfig_path.c
LDDIR?= $(.CURDIR)/..
CFLAGS+=-Wall
CFLAGS+=-I$(.CURDIR) -I$(.CURDIR)/..
LDSTATIC=${STATIC}
BINDIR=	/sbin

#.PATH: $(LDDIR) $(LDDIR)/$(MACHINE_ARCH)

.include <bsd.prog.mk>
