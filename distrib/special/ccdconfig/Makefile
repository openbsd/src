#	$OpenBSD: Makefile,v 1.1 2003/12/17 01:46:51 millert Exp $

PROG=	ccdconfig
MAN=
COPTS+=-Os -DSMALL
.PATH:  ${.CURDIR}/../../../sbin/ccdconfig

DPADD=  ${LIBUTIL}
LDADD=  -lutil

.include <bsd.prog.mk>
