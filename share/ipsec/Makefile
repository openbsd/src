#
#	$OpenBSD: Makefile,v 1.1 1997/10/09 13:43:44 mickey Exp $
#
FILES=	*.conf *.startup
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -d ${DESTDIR}${BINDIR}/ipsec
	install -c -m 0444 ${FILES} ${DESTDIR}${BINDIR}/ipsec

.include <bsd.prog.mk>
