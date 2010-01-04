#	$OpenBSD: Makefile,v 1.4 2010/01/04 17:50:39 deraadt Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93

FILES=	README

all depend includes lint tags:

realinstall:
	install -c -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} ${FILES} \
	    ${DESTDIR}${DOCDIR}

.include <bsd.subdir.mk>
