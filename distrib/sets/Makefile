# $OpenBSD: Makefile,v 1.1 2014/07/09 19:23:28 espie Exp $

DB = /usr/lib/locate/src.db

makedb:
	/bin/sh ${.CURDIR}/makelocatedb ${OSrev} >${DESTDIR}${DB}

.PHONY: makedb
