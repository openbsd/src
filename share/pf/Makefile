#
#	$OpenBSD: Makefile,v 1.6 2003/08/22 15:25:50 henning Exp $
#
FILES=	queue1 queue2 queue3 ackpri faq-example1 faq-example2 faq-example3
FILES+= spamd
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -c -m 0444 ${FILES} ${DESTDIR}${BINDIR}/pf/

.include <bsd.prog.mk>
