#	$OpenBSD: Makefile.inc,v 1.5 2001/09/04 22:25:02 deraadt Exp $

IMAGESIZE?=		65536 # 32MB in 512 byte blocks
BOOT_INSTALLBOOT?=	/usr/mdec/bootxx
GZIP?=			cat
GZIPFLAGS?=
GZIPEXT?=
