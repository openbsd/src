#	from: @(#)Makefile	5.3 (Berkeley) 6/22/90
#	$Id: Makefile,v 1.3 1995/11/08 00:01:04 deraadt Exp $

FILES=	Makefile.yp
NOOBJ=	noobj

install:
	install -c -o root -g wheel -m 644 Makefile.yp \
	    ${DESTDIR}/var/yp.no/Makefile
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/ypinit.sh ${DESTDIR}/usr/sbin/ypinit

.include <bsd.prog.mk>
