#	$OpenBSD: Makefile,v 1.1 2012/03/05 02:40:21 guenther Exp $

PROG=		main-thread-exited

LIBPTHREAD=	/usr/lib/libpthread.a

LDADD+=		-pthread
DPADD+=		${LIBPTHREAD}

.include <bsd.regress.mk>
