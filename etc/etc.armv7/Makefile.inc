#	$OpenBSD: Makefile.inc,v 1.14 2016/05/29 07:38:01 jsg Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/BOOTARM.EFI ${RELEASEDIR}

MDEXT=	bsd.rd BOOTARM.EFI \
	miniroot-am335x-${OSrev}.fs miniroot-beagle-${OSrev}.fs \
	miniroot-cubie-${OSrev}.fs miniroot-cubox-${OSrev}.fs \
	miniroot-nitrogen-${OSrev}.fs miniroot-panda-${OSrev}.fs \
	miniroot-wandboard-${OSrev}.fs
