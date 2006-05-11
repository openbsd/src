#	$OpenBSD: Makefile,v 1.5 2006/05/11 22:03:22 deraadt Exp $
#	$NetBSD: Makefile,v 1.10 1995/03/06 04:24:41 cgd Exp $

PROG=	ldconfig
SRCS=	ldconfig.c shlib.c etc.c prebind_delete.c
LDDIR?= $(.CURDIR)/..
#CFLAGS+=-Werror
#CFLAGS+=-I$(.CURDIR) -I$(LDDIR)/$(MACHINE_ARCH)
LDSTATIC=${STATIC}
BINDIR=	/sbin
MAN=	ldconfig.8

#.PATH: $(LDDIR) $(LDDIR)/$(MACHINE_ARCH)

.include <bsd.prog.mk>
