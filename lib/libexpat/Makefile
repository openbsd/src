# $OpenBSD: Makefile,v 1.3 2005/11/24 20:49:17 deraadt Exp $
.PATH: ${.CURDIR}/lib

LIB=	expat
WANTLINT=
SRCS=	xmlparse.c xmltok.c xmlrole.c
CFLAGS+=-I${.CURDIR}

includes:
	@cmp -s ${DESTDIR}/usr/include/expat.h ${.CURDIR}/lib/expat.h || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) \
	  ${.CURDIR}/lib/expat.h ${DESTDIR}/usr/include/expat.h

.include <bsd.lib.mk>
