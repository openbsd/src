#	$OpenBSD: Makefile.inc,v 1.7 1997/05/25 22:56:41 grr Exp $
#	$OpenBSD: Makefile.inc,v 1.7 1997/05/25 22:56:41 grr Exp $

IMAGESIZE?=		4352 # 2Mb & change in 512 byte blocks
BOOT_INSTALLBOOT?=	/usr/mdec/bootxx
GZIP?=			gzip
GZIPFLAGS?=		-9v
GZIPEXT?=		.gz
MAKEFLOPPY=		1
RAWLABEL=
