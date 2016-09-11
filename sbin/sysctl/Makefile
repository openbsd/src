#	$OpenBSD: Makefile,v 1.12 2016/09/11 07:06:29 natano Exp $

PROG=	sysctl
MAN=	sysctl.8

CPPFLAGS+=	-D_LIBKVM

afterinstall:
	ln -sf ../../sbin/sysctl ${DESTDIR}/usr/sbin
	chown -h ${BINOWN}:${BINGRP} ${DESTDIR}/usr/sbin/sysctl

.include <bsd.prog.mk>
