#	$OpenBSD: Makefile,v 1.11 2016/05/04 19:48:08 jca Exp $

PROG=	sysctl
MAN=	sysctl.8

CPPFLAGS+=	-D_LIBKVM

afterinstall:
	ln -sf ../../sbin/sysctl ${DESTDIR}/usr/sbin

.include <bsd.prog.mk>
