#	$OpenBSD: Makefile,v 1.5 2002/02/19 20:20:39 miod Exp $

.if ${MACHINE}=="hp300"
PROG=	hilinfo
CFLAGS+= -I${.CURDIR}/../../sys/arch/hp300
DPADD=	${LIBUTIL}
LDADD=	-lutil
.else
NOPROG=yes
.endif

MAN=hilinfo.8
MANSUBDIR=hp300

.include <bsd.prog.mk>
