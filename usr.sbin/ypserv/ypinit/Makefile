#	from: @(#)Makefile	5.3 (Berkeley) 6/22/90
#	$Id: Makefile,v 1.2 1995/11/06 21:35:06 deraadt Exp $

FILES=	Makefile.yp
NOOBJ=	noobj

install:
	install -c -o root -g wheel -m 644 Makefile.yp ${DESTDIR}/var/yp/Makefile
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/ypinit.sh ${DESTDIR}/usr/sbin/ypinit

.include <bsd.prog.mk>
