#	from: @(#)Makefile	5.14 (Berkeley) 9/15/92
#	$Id: Makefile,v 1.2 1996/01/29 01:28:45 deraadt Exp $

MAN=	termcap.5

CLEANFILES= termcap.db
all: termcap.db

termcap.db: termcap.src
	cat ${.ALLSRC} | sed \
	    -e 's,/usr/share/lib/tabset,/usr/share/tabset,g' \
	    -e 's,/usr/lib/tabset,/usr/share/tabset,g' \
	    > ${.OBJDIR}/fixed
	cap_mkdb -f termcap ${.OBJDIR}/fixed

realinstall:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 termcap.db \
	     ${DESTDIR}${BINDIR}/misc/termcap.db
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/termcap.src \
	     ${DESTDIR}${BINDIR}/misc/termcap
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/map3270 \
	     ${DESTDIR}${BINDIR}/misc/map3270

CLEANFILES+= fixed

.include <bsd.prog.mk>
