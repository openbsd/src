#	$OpenBSD: Makefile,v 1.6 1997/09/21 11:48:31 deraadt Exp $

PROG=	calendar
SRCS=   calendar.c io.c day.c ostern.c paskha.c
INTER=	de_DE.ISO_8859-1 hr_HR.ISO_8859-2 ru_SU.KOI8-R

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/calendars/calendar.* ${DESTDIR}/usr/share/calendar
.for lang in ${INTER}
	@test -d ${DESTDIR}/usr/share/calendar/${lang} || \
		mkdir ${DESTDIR}/usr/share/calendar/${lang}
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
    		${.CURDIR}/calendars/${lang}/calendar.* \
		${DESTDIR}/usr/share/calendar/${lang}; 
.endfor

.include <bsd.prog.mk>
