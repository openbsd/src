#	$OpenBSD: Makefile,v 1.5 1997/08/29 22:28:22 mickey Exp $

PROG=	installboot
MAN=	installboot.8

LDADD=	-lutil
DPADD=	${LIBUTIL}
# Need this to work in the miniroot
LDSTATIC=	-static

SADIR=	${.CURDIR}/..

.include <bsd.prog.mk>
