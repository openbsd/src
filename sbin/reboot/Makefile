#	$OpenBSD: Makefile,v 1.6 2000/11/09 03:57:32 deraadt Exp $

PROG=	reboot
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	reboot.8
MLINKS=	reboot.8 halt.8
LINKS=	${BINDIR}/reboot ${BINDIR}/halt

.include <bsd.prog.mk>
