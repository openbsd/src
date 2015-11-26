#	$OpenBSD: Makefile,v 1.14 2015/11/26 19:01:47 deraadt Exp $

.include <bsd.own.mk>

PROG=	chpass
SRCS=	chpass.c edit.c field.c table.c util.c getpwent.c
BINOWN=	root
BINMODE=4555
.PATH:	${.CURDIR}/../../lib/libc/gen
LINKS=	${BINDIR}/chpass ${BINDIR}/chfn ${BINDIR}/chpass ${BINDIR}/chsh
MLINKS=	chpass.1 chfn.1 chpass.1 chsh.1
CFLAGS+=-I${.CURDIR}/../../lib/libc/include
DPADD+= ${LIBUTIL}
LDADD+= -lutil

.include <bsd.prog.mk>

getpwent.o: getpwent.c
	${COMPILE.c} -UYP -D'DEF_WEAK(x)=asm("")' \
	    ${.CURDIR}/../../lib/libc/gen/getpwent.c
