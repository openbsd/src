#
#	$OpenBSD: Makefile,v 1.2 2002/12/15 18:34:54 henning Exp $
#
FILES=	queue1 queue2
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -c -m 0444 ${FILES} ${DESTDIR}${BINDIR}/pf/

.include <bsd.prog.mk>
