#	$OpenBSD: Makefile,v 1.4 2000/02/12 15:26:40 espie Exp $

.if ${MACHINE} == "amiga"
PROG=iteconfig
CFLAGS+=-I${.CURDIR}/../../sys/arch -D${MACHINE}
CFLAGS+=-W -Wall -Wstrict-prototypes
.else
NOPROG=yes
.endif

MAN=	iteconfig.8
MANSUBDIR=/amiga
.include <bsd.prog.mk>
