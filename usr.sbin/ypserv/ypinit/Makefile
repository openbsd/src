#	$OpenBSD: Makefile,v 1.8 1996/05/30 09:53:15 deraadt Exp $

NOOBJ=	noobj

install:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/ypinit.sh ${DESTDIR}/usr/sbin/ypinit
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/Makefile.yp ${DESTDIR}/var/yp/Makefile.yp
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/Makefile.main ${DESTDIR}/var/yp/Makefile.main

.include <bsd.prog.mk>
