#	$OpenBSD: Makefile.inc,v 1.16 2017/09/01 04:40:40 jsg Exp $

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/BOOTARM.EFI ${RELEASEDIR}

MDEXT=	bsd.rd BOOTARM.EFI \
	miniroot-am335x-${OSrev}.fs \
	miniroot-cubie-${OSrev}.fs miniroot-cubox-${OSrev}.fs \
	miniroot-nitrogen-${OSrev}.fs miniroot-panda-${OSrev}.fs \
	miniroot-wandboard-${OSrev}.fs
