#	$OpenBSD: Makefile,v 1.8 2001/06/27 06:16:46 art Exp $

PROG=	sysctl
MAN=	sysctl.8

CPPFLAGS+=	-DINET6

afterinstall:
	(cd ${DESTDIR}/usr/sbin; ln -sf ../../sbin/sysctl .)

.include <bsd.prog.mk>
