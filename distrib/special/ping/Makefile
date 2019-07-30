#	$OpenBSD: Makefile,v 1.2 2015/10/30 13:39:25 deraadt Exp $

PROG=	ping
COPTS+=	-DSMALL 
.PATH:  ${.CURDIR}/../../../sbin/ping

LDADD=	-lm
DPADD=	${LIBM}

.include <bsd.prog.mk>
