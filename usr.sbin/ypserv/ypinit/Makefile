#	$OpenBSD: Makefile,v 1.9 1996/12/08 15:00:08 downsj Exp $

NOOBJ=	noobj

install:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/ypinit.sh ${DESTDIR}/usr/sbin/ypinit
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/Makefile.yp ${DESTDIR}/var/yp/Makefile.yp
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/Makefile.main ${DESTDIR}/var/yp/Makefile.main

.include <bsd.prog.mk>
