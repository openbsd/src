# $OpenBSD: Makefile,v 1.18 2023/05/22 15:20:16 tb Exp $

LDADD +=	-Wl,-rpath,/usr/local/lib/eopenssl11 -L/usr/local/lib/eopenssl11
CFLAGS +=	-I${.CURDIR}/ -I/usr/local/include/eopenssl11/

.PATH:		${.CURDIR}/..

.include <bsd.regress.mk>
