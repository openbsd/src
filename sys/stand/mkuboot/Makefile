#	$OpenBSD: Makefile,v 1.1 2010/02/12 17:30:41 mk Exp $

MAN=	mkuboot.8

PROG=	mkuboot
DPADD=	${LIBZ}
LDADD=	-lz
CFLAGS=	-DMACHINE_ARCH=\"${MACHINE_ARCH}\"

.include <bsd.prog.mk>
