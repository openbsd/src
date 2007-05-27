#	$OpenBSD: Makefile,v 1.9 2007/05/27 21:56:57 bluhm Exp $

PROG=	sysctl
MAN=	sysctl.8

CPPFLAGS+=	-DINET6

afterinstall:
	ln -sf ../../sbin/sysctl ${DESTDIR}/usr/sbin

.include <bsd.prog.mk>
