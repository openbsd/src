# $OpenBSD: Makefile,v 1.4 1996/09/27 15:49:03 millert Exp $

PROG= skeyinit
BINOWN=root
BINMODE=4555
#DPADD=	${LIBCRYPT} ${LIBSKEY}
DPADD=	${LIBSKEY}
#LDADD=	-lcrypt -lskey
LDADD=	-lskey

.include <bsd.prog.mk>
