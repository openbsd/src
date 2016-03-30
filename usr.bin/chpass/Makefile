#	$OpenBSD: Makefile,v 1.15 2016/03/30 06:38:45 jmc Exp $

.include <bsd.own.mk>

PROG=	chpass
SRCS=	chpass.c edit.c field.c table.c util.c getpwent.c
BINOWN=	root
BINMODE=4555
.PATH:	${.CURDIR}/../../lib/libc/gen
LINKS=	${BINDIR}/chpass ${BINDIR}/chfn ${BINDIR}/chpass ${BINDIR}/chsh
CFLAGS+=-I${.CURDIR}/../../lib/libc/include
DPADD+= ${LIBUTIL}
LDADD+= -lutil

.include <bsd.prog.mk>

getpwent.o: getpwent.c
	${COMPILE.c} -UYP -D'DEF_WEAK(x)=asm("")' \
	    ${.CURDIR}/../../lib/libc/gen/getpwent.c
