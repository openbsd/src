#	$OpenBSD: Makefile,v 1.1 2014/12/23 17:16:03 deraadt Exp $

PROG=	reboot
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	reboot.8
MLINKS=	reboot.8 halt.8
LINKS=	${BINDIR}/reboot ${BINDIR}/halt

.PATH:  ${.CURDIR}/../../../sbin/reboot
.include <bsd.prog.mk>
