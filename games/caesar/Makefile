#	$OpenBSD: Makefile,v 1.5 1998/02/26 07:58:07 deraadt Exp $

PROG=	caesar
MAN=	caesar.6
DPADD=	${LIBM}
LDADD=	-lm
MLINKS=	caesar.6 rot13.6

beforeinstall:
	(cd ${DESTDIR}/usr/games; rm -f rot13; ln -s caesar rot13)

.include <bsd.prog.mk>
