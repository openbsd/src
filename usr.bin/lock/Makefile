#	$OpenBSD: Makefile,v 1.8 1999/03/06 20:27:39 millert Exp $

PROG=	lock
BINOWN= root
BINMODE=4555

.include <bsd.own.mk>

.if (${SKEY} == "yes")
CFLAGS+=-DSKEY
LDADD+= -lskey
DPADD+= ${LIBSKEY}
.endif

.include <bsd.prog.mk>
