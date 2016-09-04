#	$OpenBSD: Makefile,v 1.10 2016/09/04 09:54:25 natano Exp $

.PATH: ${.CURDIR}/lib

LIB=	expat
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

${PC_FILES}: lib/expat.h
	/bin/sh ${.CURDIR}/generate_pkgconfig.sh -c ${.CURDIR} -o ${.OBJDIR}

beforeinstall: ${PC_FILES}
	${INSTALL} ${INSTALL_COPY} -o root -g ${SHAREGRP} \
	    -m ${SHAREMODE} ${.OBJDIR}/${PC_FILES} ${DESTDIR}/usr/lib/pkgconfig/

.include <bsd.lib.mk>
