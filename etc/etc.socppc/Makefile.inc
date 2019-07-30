#	$OpenBSD: Makefile.inc,v 1.8 2017/04/04 00:03:12 deraadt Exp $

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}/boot
	cp -p ${DESTDIR}/usr/mdec/boot.elf ${RELEASEDIR}/boot.elf

MDEXT=	bsd.bin bsd.rd boot boot.elf miniroot${OSrev}.fs
