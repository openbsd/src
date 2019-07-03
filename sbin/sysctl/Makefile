#	$OpenBSD: Makefile,v 1.13 2019/07/03 10:32:33 dlg Exp $

PROG=	sysctl
MAN=	sysctl.8
DEBUG=-g

CPPFLAGS+=	-D_LIBKVM

afterinstall:
	ln -sf ../../sbin/sysctl ${DESTDIR}/usr/sbin
	chown -h ${BINOWN}:${BINGRP} ${DESTDIR}/usr/sbin/sysctl

.include <bsd.prog.mk>
