#	$OpenBSD: Makefile,v 1.3 1996/12/08 14:32:35 downsj Exp $

PROG=	strip

realinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 600 strip \
	    ${DESTDIR}${BINDIR}
	./strip ${DESTDIR}${BINDIR}/strip
	chmod ${BINMODE} ${DESTDIR}${BINDIR}/strip
	-if [ "${COPY}" = "" ]; then rm -f ./strip; fi

.include <bsd.prog.mk>
