#	$OpenBSD: Makefile.inc,v 1.14 2002/05/01 19:21:28 mickey Exp $

IMAGESIZE?=		4352
GZIP?=			gzip
GZIPFLAGS?=		-9v
GZIPEXT?=		.gz
MAKEFLOPPY=		1
RAWLABEL=
BOOT=			${DESTDIR}/usr/mdec/boot
INSTALLBOOT=		${DESTDIR}/usr/mdec/installboot
