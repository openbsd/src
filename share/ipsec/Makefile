#
#	$OpenBSD: Makefile,v 1.4 2002/09/06 22:06:29 deraadt Exp $
#
FILES=	rc.vpn
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -c -m 0444 ${FILES} ${DESTDIR}${BINDIR}/ipsec

.include <bsd.prog.mk>
