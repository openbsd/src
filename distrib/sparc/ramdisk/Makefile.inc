#	$OpenBSD: Makefile.inc,v 1.9 1997/05/29 09:19:50 grr Exp $
#	$OpenBSD: Makefile.inc,v 1.9 1997/05/29 09:19:50 grr Exp $

IMAGESIZE?=		4352 # 2Mb & change in 512 byte blocks
GZIP?=			gzip
GZIPFLAGS?=		-9v
GZIPEXT?=		.gz
MAKEFLOPPY=		1
RAWLABEL=
BOOT=			${DESTDIR}/usr/mdec/zboot
