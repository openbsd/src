#
#	$OpenBSD: Makefile,v 1.4 2003/03/02 12:19:57 dhartmei Exp $
#
FILES=	queue1 queue2 queue3 ackpri
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -c -m 0444 ${FILES} ${DESTDIR}${BINDIR}/pf/

.include <bsd.prog.mk>
