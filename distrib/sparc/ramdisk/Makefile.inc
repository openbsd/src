#	$OpenBSD: Makefile.inc,v 1.17 2007/06/17 00:28:21 deraadt Exp $

IMAGESIZE?=		4352
GZIPCMD?=		gzip
GZIPFLAGS?=		-9nv
GZIPEXT?=		.gz
MAKEFLOPPY=		1
BOOT=			${DESTDIR}/usr/mdec/boot
INSTALLBOOT=		${DESTDIR}/usr/mdec/installboot
