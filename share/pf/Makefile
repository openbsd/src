#
#	$OpenBSD: Makefile,v 1.1 2002/11/22 21:43:17 henning Exp $
#
FILES=	queue1
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -c -m 0444 ${FILES} ${DESTDIR}${BINDIR}/pf/

.include <bsd.prog.mk>
