#	$OpenBSD: Makefile,v 1.3 1996/12/08 14:32:37 downsj Exp $
#	$NetBSD: Makefile,v 1.5 1994/12/23 21:11:56 pk Exp $

PROG=	vfontedpr
SRCS=	regexp.c vfontedpr.c
MAN=	vgrind.1 vgrindefs.5
BINDIR=	/usr/libexec
CLEANFILES+=vgrindefs.src.db

all: vfontedpr vgrindefs.src.db

vgrindefs.src.db: vgrindefs.src
	cap_mkdb -f vgrindefs.src ${.CURDIR}/vgrindefs.src

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/vgrind.sh ${DESTDIR}/usr/bin/vgrind
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/vgrindefs.src ${DESTDIR}/usr/share/misc/vgrindefs
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    vgrindefs.src.db ${DESTDIR}/usr/share/misc/vgrindefs.db
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/tmac.vgrind ${DESTDIR}/usr/share/tmac

.include <bsd.prog.mk>
