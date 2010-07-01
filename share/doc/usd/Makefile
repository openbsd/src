#	$OpenBSD: Makefile,v 1.10 2010/07/01 20:08:54 tedu Exp $

DOCDIR=	/usr/share/doc/usd
FILES=	Makefile

beforeinstall:
	install -c -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} ${FILES} \
	    ${DESTDIR}${DOCDIR}

.include <bsd.subdir.mk>
