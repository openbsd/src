#	$OpenBSD: Makefile,v 1.9 2000/09/03 18:41:17 espie Exp $

PROG=	lock
BINOWN= root
BINMODE=4555

.include <bsd.own.mk>

.if (${SKEY:L} == "yes")
CFLAGS+=-DSKEY
LDADD+= -lskey
DPADD+= ${LIBSKEY}
.endif

.include <bsd.prog.mk>
