#	$OpenBSD: Makefile.inc,v 1.7 2011/07/22 09:30:54 espie Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}/boot
	cp ${DESTDIR}/usr/mdec/boot.elf ${RELEASEDIR}/boot.elf

MDEXT=	bsd.bin bsd.rd boot boot.elf miniroot${OSrev}.fs
