#	$OpenBSD: Makefile.inc,v 1.8 1997/05/28 22:43:59 niklas Exp $

IMAGESIZE?=		4352 # 2Mb & change in 512 byte blocks
BOOT_INSTALLBOOT?=	/usr/mdec/bootxx
GZIP?=			gzip
GZIPFLAGS?=		-9v
GZIPEXT?=		.gz
MAKEFLOPPY=		1
RAWLABEL=
