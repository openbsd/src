#	$OpenBSD: Makefile,v 1.2 1996/06/02 04:53:33 tholo Exp $

LKM=	ap
SRCS+=	aperture.c lkm.c version.c
NOMAN=	noman

.ifmake !(install)
SUBDIR=	aptest
.endif

afterinstall:
	install -c -o ${LKMOWN} -g ${LKMGRP} -m 555 ${.CURDIR}/apinstall \
		${DESTDIR}${LKMDIR}

.include <bsd.lkm.mk>
