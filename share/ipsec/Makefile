#
#	$OpenBSD: Makefile,v 1.3 1998/07/30 08:14:36 provos Exp $
#
FILES=	*.conf *.startup rc.vpn
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -c -m 0444 ${FILES} ${DESTDIR}${BINDIR}/ipsec

.include <bsd.prog.mk>
