#	$OpenBSD: Makefile,v 1.2 2016/03/30 06:38:40 jmc Exp $

PROG=	reboot
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	reboot.8
LINKS=	${BINDIR}/reboot ${BINDIR}/halt

.PATH:  ${.CURDIR}/../../../sbin/reboot
.include <bsd.prog.mk>
