#	$OpenBSD: Makefile,v 1.8 2000/05/26 11:50:46 deraadt Exp $

MAN=	installboot.8
MANSUBDIR=/i386

.if ${MACHINE} == "i386"
PROG=	installboot

LDADD=	-lutil
DPADD=	${LIBUTIL}
# Need this to work in the miniroot
LDSTATIC=	-static

SADIR=	${.CURDIR}/..
.else
NOPROG=
.endif

.include <bsd.prog.mk>
