# $OpenBSD: Makefile,v 1.2 2006/11/27 17:01:56 ckuethe Exp $

SCRIPT=pkg-config

# Nothing to build or clean
all:
clean:

install: 
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/${SCRIPT} ${DESTDIR}${BINDIR}/

.include <bsd.prog.mk>
