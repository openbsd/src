#	$OpenBSD: Makefile,v 1.4 1996/06/26 05:35:55 deraadt Exp $

PROG=	lock
BINOWN= root
BINMODE=4555
#LDADD+= -lcrypt
#DPADD+= ${LIBCRYPT}

.include <bsd.own.mk>

.if defined(SKEY)
CFLAGS+=-DSKEY
LDADD+= -lskey
DPADD+= ${LIBSKEY}
.endif

.include <bsd.prog.mk>
