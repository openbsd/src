#	$OpenBSD: Makefile,v 1.9 2010/07/01 20:02:28 tedu Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93

DOCDIR=	/usr/share/doc/smm
FILES=	Makefile

beforeinstall:
	install -c -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} ${FILES} \
	    ${DESTDIR}${DOCDIR}

.include <bsd.subdir.mk>
