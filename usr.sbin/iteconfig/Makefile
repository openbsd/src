#	$OpenBSD: Makefile,v 1.2 1997/09/21 11:43:48 deraadt Exp $

PROG=iteconfig
MAN=	iteconfig_${MACHINE}.8
MLINKS=	iteconfig_${MACHINE}.8 iteconfig.8
CFLAGS+=-I${.CURDIR}/../../sys/arch -D${MACHINE}

.include <bsd.prog.mk>
