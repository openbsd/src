#	$OpenBSD: Makefile,v 1.1 1996/07/22 04:46:01 tholo Exp $

#MAN=	terminfo.5

CLEANFILES= fixed terminfo.db
all: terminfo.db

terminfo.db: terminfo.src
	cat ${.ALLSRC} | sed \
	    -e 's,/usr/share/lib/tabset,/usr/share/tabset,g' \
	    -e 's,/usr/lib/tabset,/usr/share/tabset,g' \
	    > ${.OBJDIR}/fixed
	info_mkdb -f terminfo ${.OBJDIR}/fixed

realinstall:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 terminfo.db \
	     ${DESTDIR}${BINDIR}/misc/terminfo.db

.include <bsd.prog.mk>
