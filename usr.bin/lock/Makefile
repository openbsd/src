#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.3 1996/02/15 05:02:23 etheisen Exp $

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
