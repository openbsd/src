#	from: @(#)Makefile	5.5 (Berkeley) 5/11/90
#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:46:09 deraadt Exp $

PROG=	su
BINOWN=	root
BINMODE=4555
CFLAGS+=-DSKEY
LDADD+= -lcrypt -lskey
DPADD+= ${LIBCRYPT} ${LIBSKEY}

.include <bsd.prog.mk>
