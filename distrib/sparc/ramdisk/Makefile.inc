#	$OpenBSD: Makefile.inc,v 1.5 1997/05/24 10:46:21 johns Exp $
#	$OpenBSD: Makefile.inc,v 1.5 1997/05/24 10:46:21 johns Exp $

IMAGESIZE?=		8192 # 4Mb in 512 byte blocks
BOOT_INSTALLBOOT?=	/usr/mdec/bootxx
GZIP?=			gzip
GZIPFLAGS?=		-9v
GZIPEXT?=		.gz
MAKEFLOPPY=		1
RAWLABEL=
