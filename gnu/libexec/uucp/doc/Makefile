#	$OpenBSD: Makefile,v 1.1 2000/12/21 13:44:25 naddy Exp $

INFO = uucp.info uucp.info-1 uucp.info-2 uucp.info-3 uucp.info-4 \
       uucp.info-5 uucp.info-6 uucp.info-7 uucp.info-8
SRCS = uucp.texi

all: ${INFO}

${INFO}: ${SRCS}
	makeinfo ${.CURDIR}/${SRCS}

clean cleandir:
	-rm -f ${INFO}

install: ${INFO}
	 ${INSTALL} ${INSTALL_COPY} -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} ${INFO} ${DESTDIR}${SHAREDIR}/info
	 install-info --info-dir=${DESTDIR}${SHAREDIR}/info uucp.info

.include <bsd.own.mk>
.include <bsd.obj.mk>
.include <bsd.subdir.mk>
