#	$OpenBSD: Makefile,v 1.6 2000/05/25 20:32:57 deraadt Exp $

PROG=	installboot
MAN=	installboot.8
MANSUBDIR=i386

LDADD=	-lutil
DPADD=	${LIBUTIL}
# Need this to work in the miniroot
LDSTATIC=	-static

SADIR=	${.CURDIR}/..

.include <bsd.prog.mk>
