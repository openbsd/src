#	$OpenBSD: Makefile,v 1.6 1997/04/27 20:56:54 millert Exp $

PROG=	strip
.if ${HOSTCC} != ${CC}
STRIP=./hoststrip

realinstall: hoststrip

hoststrip: strip.c
	${HOSTCC} ${.CURDIR}/strip.c -o hoststrip
.else
STRIP=./strip
.endif

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 600 strip \
	    ${DESTDIR}${BINDIR}
	${STRIP} ${DESTDIR}${BINDIR}/strip
	chmod ${BINMODE} ${DESTDIR}${BINDIR}/strip
	-if [ "${INSTALL_COPY}" = "" ]; then rm -f ${STRIP}; fi

.include <bsd.prog.mk>
