#
#	$OpenBSD: Makefile,v 1.3 2002/12/17 20:12:15 henning Exp $
#
FILES=	queue1 queue2 queue3
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -c -m 0444 ${FILES} ${DESTDIR}${BINDIR}/pf/

.include <bsd.prog.mk>
