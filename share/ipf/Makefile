#
#	$Id: Makefile,v 1.1 1996/01/09 09:29:32 dm Exp $
#
FILES=	example.*
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -d ${DESTDIR}${BINDIR}/ipf
	install -c -m 0444 ${FILES} ${DESTDIR}${BINDIR}/ipf

.include <bsd.prog.mk>
