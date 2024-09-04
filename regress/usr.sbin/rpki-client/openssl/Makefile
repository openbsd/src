# $OpenBSD: Makefile,v 1.1 2024/09/04 04:34:14 tb Exp $

EOPENSSL =	eopenssl32

LDADD +=	-Wl,-rpath,/usr/local/lib/${EOPENSSL} -L/usr/local/lib/${EOPENSSL}
CFLAGS +=	-I${.CURDIR}/ -I/usr/local/include/${EOPENSSL}/

.PATH:		${.CURDIR}/..

.include <bsd.regress.mk>
