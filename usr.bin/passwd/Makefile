#	$OpenBSD: Makefile,v 1.43 2026/03/07 18:20:26 deraadt Exp $

.include <bsd.own.mk>

PROG=	passwd
SRCS=	local_passwd.c passwd.c getpwent.c \
	pwd_check.c
.PATH:  ${.CURDIR}/../chpass
DPADD+= ${LIBRPCSVC} ${LIBUTIL}
LDADD+= -lrpcsvc -lutil
CFLAGS+= -I${.CURDIR} -DFORCE_DBOPEN

BINMODE=4555
BINOWN=root

.include <bsd.prog.mk>
