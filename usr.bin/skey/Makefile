# $Id: Makefile,v 1.1.1.1 1995/10/18 08:46:07 deraadt Exp $

PROG= skey
CFLAGS+= -I${.CURDIR}/../../lib/libskey
MAN= skey.1 skeyinfo.1 skeyaudit.1
DPADD=	${LIBSKEY}
LDADD=	-lskey

beforeinstall:
	install -c -m 755 ${.CURDIR}/skeyaudit.sh ${DESTDIR}${BINDIR}/skeyaudit
	install -c -m 755 ${.CURDIR}/skeyinfo.sh ${DESTDIR}${BINDIR}/skeyinfo

.include <bsd.prog.mk>
