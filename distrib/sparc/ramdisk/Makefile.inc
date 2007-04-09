#	$OpenBSD: Makefile.inc,v 1.16 2007/04/09 14:55:11 millert Exp $

IMAGESIZE?=		4352
GZIPCMD?=		gzip
GZIPFLAGS?=		-9nv
GZIPEXT?=		.gz
MAKEFLOPPY=		1
RAWLABEL=
BOOT=			${DESTDIR}/usr/mdec/boot
INSTALLBOOT=		${DESTDIR}/usr/mdec/installboot
