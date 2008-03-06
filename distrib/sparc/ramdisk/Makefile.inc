#	$OpenBSD: Makefile.inc,v 1.18 2008/03/06 08:23:49 deraadt Exp $

IMAGESIZE?=		4000
GZIPCMD?=		gzip
GZIPFLAGS?=		-9nv
GZIPEXT?=		.gz
MAKEFLOPPY=		1
BOOT=			${DESTDIR}/usr/mdec/boot
INSTALLBOOT=		${DESTDIR}/usr/mdec/installboot
