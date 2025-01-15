# $OpenBSD: Makefile,v 1.3 2025/01/15 11:00:09 tb Exp $

EOPENSSL =	eopenssl33

LDADD +=	-Wl,-rpath,/usr/local/lib/${EOPENSSL} -L/usr/local/lib/${EOPENSSL}
CFLAGS +=	-I${.CURDIR}/ -I/usr/local/include/${EOPENSSL}/
CFLAGS +=	-DOPENSSL_SUPPRESS_DEPRECATED

.PATH:		${.CURDIR}/..

.include <bsd.regress.mk>
