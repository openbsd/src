#	$OpenBSD: Makefile,v 1.7 2000/09/03 18:41:16 espie Exp $

PROG=	sysctl
MAN=	sysctl.8

CPPFLAGS+=	-DINET6

afterinstall:
	(cd ${DESTDIR}/usr/sbin; ln -sf ../../sbin/sysctl .)

.include <bsd.prog.mk>

.if (${UVM:L} == "yes")
CFLAGS+=-DUVM
.endif
