#	$OpenBSD: Makefile,v 1.3 1999/05/10 16:14:07 espie Exp $

PROG=	ranlib
SRCS=	archive.c build.c misc.c ranlib.c touch.c
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../ar -I${.CURDIR}/../nm
MAN=	ranlib.1 ranlib.5
VPATH=	${.CURDIR}/../ar

.include <bsd.prog.mk>
