#	$OpenBSD: Makefile,v 1.11 2008/06/24 14:27:26 deraadt Exp $

.include <bsd.own.mk>

PROG=	chpass
SRCS=	chpass.c edit.c field.c pw_yp.c table.c util.c getpwent.c
BINOWN=	root
BINMODE=4555
.PATH:	${.CURDIR}/../../lib/libc/gen
LINKS=	${BINDIR}/chpass ${BINDIR}/chfn ${BINDIR}/chpass ${BINDIR}/chsh
MLINKS=	chpass.1 chfn.1 chpass.1 chsh.1
.if (${YP:L} == "yes")
CFLAGS+=-DYP -I${.CURDIR}/../../lib/libc/yp -I${.CURDIR}/../../lib/libc/include
.endif
DPADD+= ${LIBRPCSVC} ${LIBUTIL}
LDADD+= -lrpcsvc -lutil

.include <bsd.prog.mk>

getpwent.o: getpwent.c
	${COMPILE.c} -UYP ${.CURDIR}/../../lib/libc/gen/getpwent.c
