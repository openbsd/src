#
#	$Id: Makefile,v 1.2 1996/07/18 05:57:20 dm Exp $
#
FILES=	example.* nat.* firewall.*
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -d ${DESTDIR}${BINDIR}/ipf
	install -c -m 0444 ${FILES} ${DESTDIR}${BINDIR}/ipf

.include <bsd.prog.mk>
