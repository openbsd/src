#	$OpenBSD: Makefile,v 1.8 2000/09/03 18:41:17 espie Exp $

.include <bsd.own.mk>

PROG=	chpass
SRCS=	chpass.c edit.c field.c pw_yp.c table.c util.c getpwent.c
BINOWN=	root
BINMODE=4555
.PATH:	${.CURDIR}/../../lib/libc/gen
LINKS=	${BINDIR}/chpass ${BINDIR}/chfn ${BINDIR}/chpass ${BINDIR}/chsh
MLINKS=	chpass.1 chfn.1 chpass.1 chsh.1
.if (${YP:L} == "yes")
CFLAGS+=-DYP -I${.CURDIR}/../../lib/libc/yp
.endif
DPADD+= ${LIBRPCSVC} ${LIBUTIL}
LDADD+= -lrpcsvc -lutil

.include <bsd.prog.mk>

getpwent.o: getpwent.c
	${COMPILE.c} -UYP ${.IMPSRC}
