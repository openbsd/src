#	$OpenBSD $

MAN=	sysupgrade.8

SCRIPT=	sysupgrade.sh

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/${SCRIPT} ${DESTDIR}${BINDIR}/sysupgrade

.include <bsd.prog.mk>
