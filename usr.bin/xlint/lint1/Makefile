#	$OpenBSD: Makefile,v 1.4 2004/01/05 02:55:28 espie Exp $
#	$NetBSD: Makefile,v 1.3 1995/07/04 01:53:05 cgd Exp $

PROG=	lint1
SRCS=	cgram.c scan.c mem1.c mem.c err.c main1.c decl.c tree.c func.c \
	init.c emit.c emit1.c
NOMAN=
LDADD+=	-ll
DPADD+=	${LIBL}
YFLAGS=	-d
CFLAGS+=-I. -I${.CURDIR}
LINTFLAGS=-aehpz
CLEANFILES+=y.tab.h cgram.c scan.c

BINDIR=	/usr/libexec

 XXX: -O causes the gcc to die on the i386, when compiling tree.o
CFLAGS+= -DXXX_BROKEN_GCC
.if (${MACHINE_ARCH} == "sparc64")
CFLAGS+= -O0
.endif

.include <bsd.prog.mk>
