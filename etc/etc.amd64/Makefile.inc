#	$OpenBSD: Makefile.inc,v 1.17 2015/09/07 01:41:26 deraadt Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp ${DESTDIR}/usr/mdec/pxeboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/cdboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/cdbr ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/BOOTX64.EFI ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/BOOTIA32.EFI ${RELEASEDIR}

MDEXT=	bsd.rd cd${OSrev}.iso floppy${OSrev}.fs miniroot${OSrev}.fs \
	pxeboot cdboot cdbr BOOTX64.EFI BOOTIA32.EFI
