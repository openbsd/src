#	$OpenBSD: Makefile,v 1.6 2000/01/01 22:38:18 deraadt Exp $

PROG=	sysctl
MAN=	sysctl.8

CPPFLAGS+=	-DINET6

afterinstall:
	(cd ${DESTDIR}/usr/sbin; ln -sf ../../sbin/sysctl .)

.include <bsd.prog.mk>

.if (${UVM} == "yes")
CFLAGS+=-DUVM
.endif
