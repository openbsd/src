# $OpenBSD: Makefile,v 1.3 1996/06/26 05:39:23 deraadt Exp $

PROG= skeyinit
BINOWN=root
BINMODE=4555
CFLAGS+= -I${.CURDIR}/../../lib/libskey
#DPADD=	${LIBCRYPT} ${LIBSKEY}
DPADD=	${LIBSKEY}
#LDADD=	-lcrypt -lskey
LDADD=	-lskey

.include <bsd.prog.mk>
