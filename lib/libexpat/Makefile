#	$OpenBSD: Makefile,v 1.8 2012/07/07 13:35:07 jasper Exp $

.PATH: ${.CURDIR}/lib

LIB=	expat
WANTLINT=
SRCS=	xmlparse.c xmltok.c xmlrole.c
CFLAGS+=-I${.CURDIR} -DHAVE_EXPAT_CONFIG_H

PC_FILES=expat.pc
CLEANFILES+=${PC_FILES}

includes:
	cmp -s ${DESTDIR}/usr/include/expat.h ${.CURDIR}/lib/expat.h || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) \
	  ${.CURDIR}/lib/expat.h ${DESTDIR}/usr/include/expat.h
	cmp -s ${DESTDIR}/usr/include/expat_external.h ${.CURDIR}/lib/expat_external.h || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) \
	  ${.CURDIR}/lib/expat_external.h ${DESTDIR}/usr/include/expat_external.h

beforeinstall:
	/bin/sh ${.CURDIR}/generate_pkgconfig.sh -c ${.CURDIR} -o ${.OBJDIR}
	${INSTALL} ${INSTALL_COPY} -o root -g ${SHAREGRP} \
	    -m ${SHAREMODE} ${.OBJDIR}/${PC_FILES} ${DESTDIR}/usr/lib/pkgconfig/

.include <bsd.lib.mk>
