#	$OpenBSD: Makefile.inc,v 1.15 2017/04/04 00:03:12 deraadt Exp $

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/BOOTARM.EFI ${RELEASEDIR}

MDEXT=	bsd.rd BOOTARM.EFI \
	miniroot-am335x-${OSrev}.fs miniroot-beagle-${OSrev}.fs \
	miniroot-cubie-${OSrev}.fs miniroot-cubox-${OSrev}.fs \
	miniroot-nitrogen-${OSrev}.fs miniroot-panda-${OSrev}.fs \
	miniroot-wandboard-${OSrev}.fs
