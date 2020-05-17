#	$OpenBSD: Makefile.inc,v 1.17 2020/05/17 17:04:29 deraadt Exp $

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/BOOTARM.EFI ${RELEASEDIR}

MDEXT=	bsd.rd BOOTARM.EFI \
	miniroot-am335x-${OSrev}.img \
	miniroot-cubie-${OSrev}.img miniroot-cubox-${OSrev}.img \
	miniroot-nitrogen-${OSrev}.img miniroot-panda-${OSrev}.img \
	miniroot-wandboard-${OSrev}.img
