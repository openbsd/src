#	$OpenBSD: Makefile,v 1.5 1997/04/27 20:56:24 millert Exp $
#	from: @(#)Makefile	5.14 (Berkeley) 9/15/92

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
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 termcap.db \
	     ${DESTDIR}${BINDIR}/misc/termcap.db
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/termcap.src \
	     ${DESTDIR}${BINDIR}/misc/termcap
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/map3270 \
	     ${DESTDIR}${BINDIR}/misc/map3270
	ln -fs ${BINDIR}/misc/termcap ${DESTDIR}/etc/termcap

CLEANFILES+= fixed

.include <bsd.prog.mk>
