.PATH: ${.CURDIR}/lib

LIB=	expat
SRCS=	xmlparse.c xmltok.c xmlrole.c
CFLAGS+=-I${.CURDIR}

includes:
	@cmp -s ${DESTDIR}/usr/include/expat.h ${.CURDIR}/lib/expat.h || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) \
	  ${.CURDIR}/lib/expat.h ${DESTDIR}/usr/include/expat.h

.include <bsd.lib.mk>
