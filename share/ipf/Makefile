#	$OpenBSD: Makefile,v 1.3 2000/03/02 14:46:34 todd Exp $
#
#
FILES=	example.* nat.* firewall.*
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -d ${DESTDIR}${BINDIR}/ipf
	install -c -m 0444 ${FILES} ${DESTDIR}${BINDIR}/ipf

.include <bsd.prog.mk>
