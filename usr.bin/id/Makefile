#	$OpenBSD: Makefile,v 1.4 1997/04/27 20:56:45 millert Exp $

PROG=	id
MAN=	id.1 groups.1 whoami.1

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/groups.sh ${DESTDIR}/usr/bin/groups
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/whoami.sh ${DESTDIR}/usr/bin/whoami

.include <bsd.prog.mk>
