#	$OpenBSD: Makefile,v 1.1 2013/10/07 15:52:18 jsing Exp $

PROG=		sigprof

LIBPTHREAD=	/usr/lib/libpthread.a

LDADD+=		-lpthread
DPADD+=		${LIBPTHREAD}

.include <bsd.regress.mk>
