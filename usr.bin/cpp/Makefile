#	$Id: Makefile,v 1.1 1996/01/02 13:49:07 deraadt Exp $

NOMAN=

beforeinstall:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/cpp.sh ${DESTDIR}${BINDIR}/cpp

.include <bsd.prog.mk>
