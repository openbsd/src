#	$OpenBSD: Makefile,v 1.3 2000/01/19 16:32:59 espie Exp $

PROG=iteconfig
MAN=	iteconfig_${MACHINE}.8
MLINKS=	iteconfig_${MACHINE}.8 iteconfig.8
CFLAGS+=-I${.CURDIR}/../../sys/arch -D${MACHINE}
CFLAGS+=-W -Wall -Wstrict-prototypes

.include <bsd.prog.mk>
