#	$OpenBSD: Makefile,v 1.6 2002/11/06 17:54:51 pvalchev Exp $

INFO = amdref.info amdref.info-1 amdref.info-2 amdref.info-3
SRCS = amdref.texinfo

all: ${INFO}

${INFO}: ${SRCS}
	-makeinfo --no-validate ${.CURDIR}/${SRCS}

clean cleandir:
	-rm -f ${INFO}

install: ${INFO}
	 ${INSTALL} ${INSTALL_COPY} -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} ${INFO} ${DESTDIR}${SHAREDIR}/info
	 install-info --info-dir=${DESTDIR}${SHAREDIR}/info amdref.info

.include <bsd.own.mk>
.include <bsd.obj.mk>
.include <bsd.subdir.mk>
