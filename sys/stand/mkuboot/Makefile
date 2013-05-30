#	$OpenBSD: Makefile,v 1.2 2013/05/30 13:50:09 bmercer Exp $

MAN=	mkuboot.8

BINDIR=	/usr/sbin
PROG=	mkuboot
DPADD=	${LIBZ}
LDADD=	-lz
CFLAGS=	-DMACHINE_ARCH=\"${MACHINE_ARCH}\"

.include <bsd.prog.mk>
