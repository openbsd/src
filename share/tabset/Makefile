#	$OpenBSD: Makefile,v 1.6 2024/08/23 15:13:58 nicm Exp $
#	from: @(#)Makefile	5.3 (Berkeley) 10/22/90

FILES=	std stdcrt vt100 vt300
CLEANFILES+= ${FILES}

all: ${FILES}

${FILES}:
	sh ${.CURDIR}/gentabset.sh ${.CURDIR}/obj

realinstall: ${FILES}
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/tabset

.include <bsd.prog.mk>
