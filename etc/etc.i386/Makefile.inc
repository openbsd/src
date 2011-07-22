#	$OpenBSD: Makefile.inc,v 1.20 2011/07/22 09:30:54 espie Exp $

KERNELS += GENERIC.MP bsd.mp

kernels: linux-notes

bootblocks:
	cp ${DESTDIR}/usr/mdec/pxeboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/cdboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/cdbr ${RELEASEDIR}

linux-notes:
	cp ${.CURDIR}/etc.i386/INSTALL.* ${RELEASEDIR}

MDEXT=	bsd.rd cd${OSrev}.iso cdemu${OSrev}.iso \
	floppy${OSrev}.fs floppyB${OSrev}.fs floppyC${OSrev}.fs \
	pxeboot cdboot cdbr INSTALL.linux
