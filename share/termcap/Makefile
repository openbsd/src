#	from: @(#)Makefile	5.14 (Berkeley) 9/15/92
#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:44:48 deraadt Exp $

MAN=	termcap.5

CLEANFILES= termcap.db
all: termcap.db

termcap.db: termcap.src
	cap_mkdb -f termcap ${.ALLSRC}

realinstall:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 termcap.db \
	     ${DESTDIR}${BINDIR}/misc/termcap.db
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/termcap.src \
	     ${DESTDIR}${BINDIR}/misc/termcap
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/map3270 \
	     ${DESTDIR}${BINDIR}/misc/map3270


.include <bsd.prog.mk>
