# $OpenBSD: Makefile,v 1.4 1996/09/28 00:00:40 millert Exp $

PROG= skey
MAN= skey.1 skeyinfo.1 skeyaudit.1 skeyprune.8
DPADD=	${LIBSKEY}
LDADD=	-lskey

beforeinstall:
	install -c -m 755 ${.CURDIR}/skeyaudit.sh ${DESTDIR}${BINDIR}/skeyaudit
	install -c -m 755 ${.CURDIR}/skeyinfo.sh ${DESTDIR}${BINDIR}/skeyinfo
	install -c -m 755 ${.CURDIR}/skeyprune.pl ${DESTDIR}${BINDIR}/skeyprune

.include <bsd.prog.mk>
