#	$OpenBSD: Makefile,v 1.4 1997/02/11 17:42:51 deraadt Exp $

PROG=	strip
.if ${HOSTCC} != ${CC}
STRIP=./hoststrip

realinstall:: hoststrip

hoststrip: strip.c
	${HOSTCC} strip.c -o hoststrip
.else
STRIP=./strip
.endif

realinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 600 strip \
	    ${DESTDIR}${BINDIR}
	${STRIP} ${DESTDIR}${BINDIR}/strip
	chmod ${BINMODE} ${DESTDIR}${BINDIR}/strip
	-if [ "${COPY}" = "" ]; then rm -f ${STRIP}; fi

.include <bsd.prog.mk>
