# $Id: Makefile,v 1.2 1996/02/15 05:02:27 etheisen Exp $

PROG= skeyinit
BINOWN=root
BINMODE=4555
CFLAGS+= -I${.CURDIR}/../../lib/libskey
#DPADD=	${LIBCRYPT} ${LIBSKEY}
DPADD=	${LIBSKEY}
#LDADD=	-lcrypt -lskey
LDADD=	-lskey

.include <bsd.prog.mk>
