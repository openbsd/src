#	$OpenBSD: Makefile,v 1.2 2013/09/04 14:54:13 patrick Exp $

.if ${MACHINE} == "armv7"

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
