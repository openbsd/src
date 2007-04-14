# $OpenBSD: Makefile,v 1.1 2007/04/14 11:54:00 grunk Exp $

MAN=mount_vnd.8

beforeinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/mount_vnd ${DESTDIR}${BINDIR}/mount_vnd

.include <bsd.prog.mk>
