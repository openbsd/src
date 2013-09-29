#	$OpenBSD: Makefile,v 1.1 2013/09/29 17:51:34 miod Exp $

MAN=		installboot.8
MANSUBDIR=	aviion

.if ${MACHINE} == "aviion"
PROG=		installboot
BINDIR=		/usr/mdec
DPADD=		${LIBUTIL}
LDADD=		-lutil
LDSTATIC=	-static
.else
NOPROG=
.endif

.include <bsd.prog.mk>
