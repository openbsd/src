#	from: @(#)Makefile	5.3 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:43:25 deraadt Exp $

CFLAGS+= -DBSDINETD
PROG=	uucpd
NOMAN=	noman
LDADD+= -lcrypt
DPADD+=	${LIBCRYPT}

.include <bsd.prog.mk>
