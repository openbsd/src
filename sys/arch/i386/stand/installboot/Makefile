#	$OpenBSD: Makefile,v 1.7 2000/05/25 21:55:01 deraadt Exp $

PROG=	installboot
MAN=	installboot.8
MANSUBDIR=/i386

LDADD=	-lutil
DPADD=	${LIBUTIL}
# Need this to work in the miniroot
LDSTATIC=	-static

SADIR=	${.CURDIR}/..

.include <bsd.prog.mk>
