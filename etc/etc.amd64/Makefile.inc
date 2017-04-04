#	$OpenBSD: Makefile.inc,v 1.18 2017/04/04 00:03:12 deraadt Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/pxeboot ${RELEASEDIR}
	cp -p ${DESTDIR}/usr/mdec/cdboot ${RELEASEDIR}
	cp -p ${DESTDIR}/usr/mdec/cdbr ${RELEASEDIR}
	cp -p ${DESTDIR}/usr/mdec/BOOTX64.EFI ${RELEASEDIR}
	cp -p ${DESTDIR}/usr/mdec/BOOTIA32.EFI ${RELEASEDIR}

MDEXT=	bsd.rd cd${OSrev}.iso floppy${OSrev}.fs miniroot${OSrev}.fs \
	pxeboot cdboot cdbr BOOTX64.EFI BOOTIA32.EFI
