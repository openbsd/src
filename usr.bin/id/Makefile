#	$OpenBSD: Makefile,v 1.3 1996/12/08 14:32:26 downsj Exp $

PROG=	id
MAN=	id.1 groups.1 whoami.1

afterinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/groups.sh ${DESTDIR}/usr/bin/groups
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/whoami.sh ${DESTDIR}/usr/bin/whoami

.include <bsd.prog.mk>
