#	$OpenBSD: Makefile,v 1.4 1996/12/08 14:32:16 downsj Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	calendar
SRCS=   calendar.c io.c day.c ostern.c paskha.c
INTER=	de_DE.ISO_8859-1 hr_HR.ISO_8859-2 ru_SU.KOI8-R

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/calendars/calendar.* ${DESTDIR}/usr/share/calendar
.for lang in ${INTER}
	@test -d ${DESTDIR}/usr/share/calendar/${lang} || \
		mkdir ${DESTDIR}/usr/share/calendar/${lang}
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
    		${.CURDIR}/calendars/${lang}/calendar.* \
		${DESTDIR}/usr/share/calendar/${lang}; 
.endfor

.include <bsd.prog.mk>
