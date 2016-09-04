# $OpenBSD: Makefile.inc,v 1.1 2016/09/04 11:13:08 schwarze Exp $

obj-clean:
	rm -rf ${.CURDIR}/obj/*
	rm -f ${.CURDIR}/obj
	cd ${.CURDIR} && ${MAKE} clean
