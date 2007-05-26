# $OpenBSD: Makefile,v 1.3 2007/05/26 21:39:45 grunk Exp $

PROG=	mount_vnd
SRCS=	mount_vnd.c pkcs5_pbkdf2.c
LDADD=	-lutil -lcrypto
DPADD=	${LIBUTIL}

LINKS=	${BINDIR}/mount_vnd ${BINDIR}/vnconfig
MLINKS=	mount_vnd.8 vnconfig.8

# XXX compatibility
afterinstall:
	(cd ${DESTDIR}/usr/sbin && ln -sf ../../sbin/vnconfig .)

MAN=	mount_vnd.8

.include <bsd.prog.mk>
