#	$OpenBSD: Makefile,v 1.6 2007/10/19 16:06:39 deraadt Exp $

.PATH: ${.CURDIR}/lib

LIB=	expat
WANTLINT=
SRCS=	xmlparse.c xmltok.c xmlrole.c
CFLAGS+=-I${.CURDIR} -DHAVE_EXPAT_CONFIG_H

includes:
	cmp -s ${DESTDIR}/usr/include/expat.h ${.CURDIR}/lib/expat.h || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) \
	  ${.CURDIR}/lib/expat.h ${DESTDIR}/usr/include/expat.h
	cmp -s ${DESTDIR}/usr/include/expat_external.h ${.CURDIR}/lib/expat_external.h || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) \
	  ${.CURDIR}/lib/expat_external.h ${DESTDIR}/usr/include/expat_external.h

.include <bsd.lib.mk>
