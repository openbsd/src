#	$OpenBSD: Makefile.inc,v 1.21 2014/03/03 01:37:36 deraadt Exp $

KERNELS += GENERIC.MP bsd.mp

kernels: linux-notes

bootblocks:
	cp ${DESTDIR}/usr/mdec/pxeboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/cdboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/cdbr ${RELEASEDIR}

linux-notes:
	cp ${.CURDIR}/etc.i386/INSTALL.* ${RELEASEDIR}

MDEXT=	bsd.rd cd${OSrev}.iso floppy${OSrev}.fs floppyB${OSrev}.fs \
	floppyC${OSrev}.fs pxeboot cdboot cdbr INSTALL.linux
