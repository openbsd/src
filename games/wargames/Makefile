#	$OpenBSD: Makefile,v 1.2 1996/12/08 15:36:19 downsj Exp $
#	$NetBSD: Makefile,v 1.4 1995/04/22 07:53:41 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

NOOBJ=	noobj
all wargames clean cleandir depend lint tags:

install:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/wargames.sh ${DESTDIR}/usr/games/wargames

.include <bsd.prog.mk>
