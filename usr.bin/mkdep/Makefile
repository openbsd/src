#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:50:02 deraadt Exp $

MAN=	mkdep.1

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/mkdep.gcc.sh ${DESTDIR}/usr/bin/mkdep

.include <bsd.prog.mk>
