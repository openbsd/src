#	$OpenBSD: Makefile,v 1.5 1999/12/30 19:06:05 provos Exp $

PROG=	sysctl
MAN=	sysctl.8

CPPFLAGS+=	-DINET6

.include <bsd.prog.mk>

.if (${UVM} == "yes")
CFLAGS+=-DUVM
.endif
