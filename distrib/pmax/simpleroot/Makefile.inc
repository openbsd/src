#	$OpenBSD: Makefile.inc,v 1.3 2000/03/01 22:10:08 todd Exp $

IMAGESIZE?=		81920 # 40Mb in 512 byte blocks
BOOT_DISKLABEL?=	-b /usr/mdec/rzboot -s /usr/mdec/bootrz
GZIP?=			cat
GZIPFLAGS?=
GZIPEXT?=
