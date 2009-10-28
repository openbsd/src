#	$OpenBSD: Makefile,v 1.2 2009/10/28 07:36:49 deraadt Exp $

PROG=	ccdconfig
COPTS+=	-DSMALL
.PATH:  ${.CURDIR}/../../../sbin/ccdconfig

DPADD=  ${LIBUTIL}
LDADD=  -lutil

.include <bsd.prog.mk>
