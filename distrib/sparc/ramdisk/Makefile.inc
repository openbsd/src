#	$OpenBSD: Makefile.inc,v 1.6 1997/05/25 08:20:32 grr Exp $
#	$OpenBSD: Makefile.inc,v 1.6 1997/05/25 08:20:32 grr Exp $

IMAGESIZE?=		4480 # 4Mb in 512 byte blocks
BOOT_INSTALLBOOT?=	/usr/mdec/bootxx
GZIP?=			gzip
GZIPFLAGS?=		-9v
GZIPEXT?=		.gz
MAKEFLOPPY=		1
RAWLABEL=
