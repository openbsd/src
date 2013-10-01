#	$OpenBSD: Makefile,v 1.1 2013/10/01 20:32:30 miod Exp $

MAN=		vdmtool.8
MANSUBDIR=	aviion

.if ${MACHINE} == "aviion"
PROG=		vdmtool
BINDIR=		/usr/mdec
DPADD=		${LIBUTIL}
LDADD=		-lutil
LDSTATIC=	-static
.else
NOPROG=
.endif

.include <bsd.prog.mk>
