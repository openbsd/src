#
#	$Id: Makefile,v 1.1 1998/02/07 21:05:50 beck Exp $
#
FILES=	example.* 
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -d ${DESTDIR}${BINDIR}/smtpd
	install -c -m 0444 ${FILES} ${DESTDIR}${BINDIR}/smtpd

.include <bsd.prog.mk>
