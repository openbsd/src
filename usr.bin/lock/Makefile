#	$NetBSD: Makefile,v 1.7 1994/12/22 01:16:19 jtc Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	lock
BINOWN= root
BINMODE=4555
CFLAGS+=-DSKEY
LDADD+= -lcrypt -lskey
DPADD+= ${LIBCRYPT} ${LIBSKEY}

.include <bsd.prog.mk>
