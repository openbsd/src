#	$OpenBSD: Makefile,v 1.5 2000/04/18 20:17:53 mickey Exp $

PROG=	nfsstat
CFLAGS+=-DNFS
DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
