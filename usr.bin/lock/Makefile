#	$NetBSD: Makefile,v 1.7 1994/12/22 01:16:19 jtc Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	lock
BINOWN= root
BINMODE=4555
LDADD+= -lcrypt
DPADD+= ${LIBCRYPT}

.include <bsd.own.mk>

.if defined(SKEY)
CFLAGS+=-DSKEY
LDADD+= -lskey
DPADD+= ${LIBSKEY}
.endif

.include <bsd.prog.mk>
