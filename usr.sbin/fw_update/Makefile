#	$OpenBSD: Makefile,v 1.1 2011/07/09 01:16:57 halex Exp $

#MAN=	fw_update.1

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/fw_update.sh ${DESTDIR}${BINDIR}/fw_update

.include <bsd.prog.mk>
