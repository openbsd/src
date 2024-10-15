# $OpenBSD: Makefile,v 1.2 2024/10/15 21:03:10 tb Exp $

EOPENSSL =	eopenssl32

LDADD +=	-Wl,-rpath,/usr/local/lib/${EOPENSSL} -L/usr/local/lib/${EOPENSSL}
CFLAGS +=	-I${.CURDIR}/ -I/usr/local/include/${EOPENSSL}/
CFLAGS +=	-DOPENSSL_SUPPRESS_DEPRECATED

.PATH:		${.CURDIR}/..

.include <bsd.regress.mk>
