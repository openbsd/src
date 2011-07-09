#	$OpenBSD: Makefile,v 1.2 2011/07/09 04:54:19 halex Exp $

MAN=	fw_update.1

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/fw_update.sh ${DESTDIR}${BINDIR}/fw_update

.include <bsd.prog.mk>
