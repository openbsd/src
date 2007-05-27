# $OpenBSD: Makefile,v 1.4 2007/05/27 21:56:57 bluhm Exp $

PROG=	mount_vnd
SRCS=	mount_vnd.c pkcs5_pbkdf2.c
LDADD=	-lutil -lcrypto
DPADD=	${LIBUTIL}

LINKS=	${BINDIR}/mount_vnd ${BINDIR}/vnconfig
MLINKS=	mount_vnd.8 vnconfig.8

# XXX compatibility
afterinstall:
	ln -sf ../../sbin/vnconfig ${DESTDIR}/usr/sbin 

MAN=	mount_vnd.8

.include <bsd.prog.mk>
