#	$OpenBSD: Makefile.inc,v 1.3 2001/09/04 22:25:02 deraadt Exp $

IMAGESIZE?=		3000 # 1.5MB & change in 512 byte blocks
GZIP?=			gzip
GZIPFLAGS?=		-9v
GZIPEXT?=		.gz
RAWLABEL=
BOOT=			${DESTDIR}/usr/mdec/ufsboot
INSTALLBOOT=		${DESTDIR}/usr/mdec/installboot
