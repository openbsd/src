# $OpenBSD: Makefile,v 1.2 2016/09/25 17:32:22 natano Exp $

DB = /usr/lib/locate/src.db

makedb:
	/bin/sh ${.CURDIR}/makelocatedb ${OSrev} >${DESTDIR}${DB}
	chown ${BINOWN}:${BINGRP} ${DESTDIR}${DB}

.PHONY: makedb

.include <bsd.own.mk>
