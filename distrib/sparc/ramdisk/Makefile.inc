#	$OpenBSD: Makefile.inc,v 1.10 1997/09/19 06:07:18 deraadt Exp $
#	$OpenBSD: Makefile.inc,v 1.10 1997/09/19 06:07:18 deraadt Exp $

IMAGESIZE?=		4352 # 2Mb & change in 512 byte blocks
GZIP?=			gzip
GZIPFLAGS?=		-9v
GZIPEXT?=		.gz
MAKEFLOPPY=		1
RAWLABEL=
BOOT=			${DESTDIR}/usr/mdec/boot
