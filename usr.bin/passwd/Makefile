#	$OpenBSD: Makefile,v 1.40 2015/09/14 07:19:41 guenther Exp $

.include <bsd.own.mk>

PROG=	passwd
SRCS=	local_passwd.c yp_passwd.c passwd.c getpwent.c \
	pwd_check.c
.PATH:  ${.CURDIR}/../../lib/libc/gen
DPADD+= ${LIBRPCSVC} ${LIBUTIL}
LDADD+= -lrpcsvc -lutil
CFLAGS+= -I${.CURDIR}

CFLAGS+=-I${.CURDIR}/../../lib/libc/include
.if (${YP:L} == "yes")
CFLAGS+=-DYP -I${.CURDIR}/../../lib/libc/yp
.endif

BINMODE=4555
BINOWN=root

getpwent.o: getpwent.c
	${COMPILE.c} -UYP -D'DEF_WEAK(x)=asm("")' \
	    ${.CURDIR}/../../lib/libc/gen/getpwent.c

.include <bsd.prog.mk>
