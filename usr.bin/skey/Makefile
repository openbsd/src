# $OpenBSD: Makefile,v 1.3 1996/09/27 15:41:35 millert Exp $

PROG= skey
MAN= skey.1 skeyinfo.1 skeyaudit.1
DPADD=	${LIBSKEY}
LDADD=	-lskey

beforeinstall:
	install -c -m 755 ${.CURDIR}/skeyaudit.sh ${DESTDIR}${BINDIR}/skeyaudit
	install -c -m 755 ${.CURDIR}/skeyinfo.sh ${DESTDIR}${BINDIR}/skeyinfo

.include <bsd.prog.mk>
