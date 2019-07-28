#	$OpenBSD: Makefile,v 1.4 2019/07/28 15:45:30 visa Exp $

SUBDIR= boot ramdisk

unconfig:
	cd boot; ${MAKE} unconfig
	cd ramdisk; ${MAKE} unconfig

.include <bsd.subdir.mk>
