#	$OpenBSD: Makefile,v 1.7 2005/12/07 01:55:12 cloder Exp $
#	$NetBSD: Makefile,v 1.3 1995/07/04 01:53:05 cgd Exp $

PROG=	lint1
SRCS=	cgram.c scan.c mem1.c mem.c err.c main1.c decl.c tree.c func.c \
	init.c emit.c emit1.c attr.c
NOMAN=
LDADD+=	-ll
DPADD+=	${LIBL}
YFLAGS=	-d
CFLAGS+=-I. -I${.CURDIR}
LINTFLAGS=-aehpz
CLEANFILES+=y.tab.h cgram.c scan.c

BINDIR=	/usr/libexec

.include <bsd.prog.mk>
