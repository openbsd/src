#	$OpenBSD: Makefile,v 1.2 2000/03/02 14:46:52 todd Exp $
#
#
FILES=	example.* 
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -d ${DESTDIR}${BINDIR}/smtpd
	install -c -m 0444 ${FILES} ${DESTDIR}${BINDIR}/smtpd

.include <bsd.prog.mk>
