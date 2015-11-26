#	$OpenBSD: Makefile,v 1.41 2015/11/26 19:01:47 deraadt Exp $

.include <bsd.own.mk>

PROG=	passwd
SRCS=	local_passwd.c passwd.c getpwent.c \
	pwd_check.c
.PATH:  ${.CURDIR}/../../lib/libc/gen
DPADD+= ${LIBRPCSVC} ${LIBUTIL}
LDADD+= -lrpcsvc -lutil
CFLAGS+= -I${.CURDIR}

CFLAGS+=-I${.CURDIR}/../../lib/libc/include

BINMODE=4555
BINOWN=root

getpwent.o: getpwent.c
	${COMPILE.c} -UYP -D'DEF_WEAK(x)=asm("")' \
	    ${.CURDIR}/../../lib/libc/gen/getpwent.c

.include <bsd.prog.mk>
