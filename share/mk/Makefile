#	$OpenBSD: Makefile,v 1.5 1996/06/03 22:59:15 niklas Exp $
#	$NetBSD: Makefile,v 1.12 1995/10/22 00:45:53 christos Exp $

FILES=	bsd.README bsd.doc.mk bsd.dep.mk bsd.lib.mk bsd.man.mk bsd.nls.mk \
	bsd.obj.mk bsd.own.mk bsd.port.mk bsd.port.subdir.mk bsd.prog.mk \
	bsd.subdir.mk bsd.sys.mk sys.mk bsd.lkm.mk
NOOBJ=	noobj

install:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/mk

.include <bsd.prog.mk>
