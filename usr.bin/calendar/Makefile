#	$NetBSD: Makefile,v 1.6 1995/03/27 08:46:11 glass Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	calendar

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/calendars/calendar.* ${DESTDIR}/usr/share/calendar

.include <bsd.prog.mk>
