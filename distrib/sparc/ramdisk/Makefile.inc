#	$OpenBSD: Makefile.inc,v 1.15 2007/02/16 16:56:22 tsi Exp $

IMAGESIZE?=		4352
GZIPCMD?=		gzip
GZIPFLAGS?=		-9v
GZIPEXT?=		.gz
MAKEFLOPPY=		1
RAWLABEL=
BOOT=			${DESTDIR}/usr/mdec/boot
INSTALLBOOT=		${DESTDIR}/usr/mdec/installboot
