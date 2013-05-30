#	$OpenBSD: Makefile,v 1.1 2013/05/30 19:17:15 bmercer Exp $

.if ${MACHINE} == "beagle"

BINDIR=	/usr/sbin
PROG=	mkuboot
DPADD=	${LIBZ}
LDADD=	-lz
CFLAGS=	-DMACHINE_ARCH=\"${MACHINE_ARCH}\"

.else

NOPROG=yes

.endif

MAN=	mkuboot.8

.include <bsd.prog.mk>
