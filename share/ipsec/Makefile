#
#	$OpenBSD: Makefile,v 1.2 1997/10/09 14:17:23 deraadt Exp $
#
FILES=	*.conf *.startup
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -c -m 0444 ${FILES} ${DESTDIR}${BINDIR}/ipsec

.include <bsd.prog.mk>
