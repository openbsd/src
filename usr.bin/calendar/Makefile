#	$OpenBSD: Makefile,v 1.10 2015/10/23 10:33:52 tedu Exp $

PROG=	calendar
SRCS=   calendar.c io.c day.c pesach.c ostern.c paskha.c
INTER=	de_DE.UTF-8 hr_HR.UTF-8 ru_RU.UTF-8 fr_FR.UTF-8

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
