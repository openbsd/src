#	$OpenBSD: Makefile,v 1.2 2001/05/14 22:18:22 niklas Exp $
#	$NetBSD: Makefile,v 1.10 1995/03/06 04:24:41 cgd Exp $

PROG=	ldconfig
SRCS=	ldconfig.c shlib.c etc.c
LDDIR?= $(.CURDIR)/..
CFLAGS+=-Werror
#CFLAGS+=-I$(.CURDIR) -I$(LDDIR)/$(MACHINE_ARCH)
LDSTATIC=-static
BINDIR=	/sbin
MAN=	ldconfig.8

#.PATH: $(LDDIR) $(LDDIR)/$(MACHINE_ARCH)

.include <bsd.prog.mk>
