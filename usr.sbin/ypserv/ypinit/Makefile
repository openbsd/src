#	from: @(#)Makefile	5.3 (Berkeley) 6/22/90
#	$Id: Makefile,v 1.6 1996/01/20 00:35:27 chuck Exp $

NOOBJ=	noobj

install:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/ypinit.sh ${DESTDIR}/usr/sbin/ypinit
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/Makefile.yp ${DESTDIR}/var/yp.no/Makefile.yp
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/Makefile.main ${DESTDIR}/var/yp.no/Makefile.main

.include <bsd.prog.mk>
