#	$OpenBSD: Makefile,v 1.2 1996/06/26 05:31:44 deraadt Exp $
#	$NetBSD: Makefile,v 1.6 1995/03/27 08:46:11 glass Exp $

PROG=	calendar

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/calendars/calendar.* ${DESTDIR}/usr/share/calendar

.include <bsd.prog.mk>
