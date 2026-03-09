#	$OpenBSD: Makefile,v 1.44 2026/03/09 12:56:12 deraadt Exp $

.include <bsd.own.mk>

PROG=	passwd
SRCS=	local_passwd.c passwd.c getpwent.c \
	pwd_check.c
.PATH:  ${.CURDIR}/../chpass
DPADD+= ${LIBRPCSVC} ${LIBUTIL}
LDADD+= -lrpcsvc -lutil
CFLAGS+= -I${.CURDIR}

BINMODE=4555
BINOWN=root

.include <bsd.prog.mk>
