#	$OpenBSD: Makefile.inc,v 1.13 2001/09/21 05:43:42 deraadt Exp $
#	$OpenBSD: Makefile.inc,v 1.13 2001/09/21 05:43:42 deraadt Exp $

IMAGESIZE?=		4352
GZIP?=			gzip
GZIPFLAGS?=		-9v
GZIPEXT?=		.gz
MAKEFLOPPY=		1
RAWLABEL=
BOOT=			${DESTDIR}/usr/mdec/boot
INSTALLBOOT=		${DESTDIR}/usr/mdec/installboot
