# $OpenBSD: Makefile,v 1.4 2025/07/16 15:21:45 tb Exp $

.if exists(/usr/local/bin/eopenssl35)
EOPENSSL =	eopenssl35
.else
EOPENSSL =	eopenssl33
.endif

LDADD +=	-Wl,-rpath,/usr/local/lib/${EOPENSSL} -L/usr/local/lib/${EOPENSSL}
CFLAGS +=	-I${.CURDIR}/ -I/usr/local/include/${EOPENSSL}/
CFLAGS +=	-DOPENSSL_SUPPRESS_DEPRECATED

.PATH:		${.CURDIR}/..

.include <bsd.regress.mk>
