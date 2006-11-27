# $OpenBSD: Makefile,v 1.1 2006/11/27 16:49:13 ckuethe Exp $

SCRIPT=pkg-config

# Nothing to build or clean
all:
clean:

install: 
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${SCRIPT} ${DESTDIR}${BINDIR}/

.include <bsd.prog.mk>
