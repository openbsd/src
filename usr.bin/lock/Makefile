#	$OpenBSD: Makefile,v 1.7 1999/03/06 20:19:19 millert Exp $

PROG=	lock
BINOWN= root
BINMODE=4555
DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.own.mk>

.if (${SKEY} == "yes")
CFLAGS+=-DSKEY
LDADD+= -lskey
DPADD+= ${LIBSKEY}
.endif

.include <bsd.prog.mk>
