#	$OpenBSD: Makefile,v 1.5 1997/03/26 00:35:27 deraadt Exp $

PROG=	lock
BINOWN= root
BINMODE=4555

.include <bsd.own.mk>

.if defined(SKEY)
CFLAGS+=-DSKEY
LDADD+= -lskey
DPADD+= ${LIBSKEY}
.endif

.include <bsd.prog.mk>
