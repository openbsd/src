#	$OpenBSD: Makefile,v 1.6 1997/04/19 21:26:26 millert Exp $

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
