#	$OpenBSD: Makefile.inc,v 1.23 2014/03/07 22:50:31 tedu Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp ${DESTDIR}/usr/mdec/pxeboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/cdboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/cdbr ${RELEASEDIR}

MDEXT=	bsd.rd cd${OSrev}.iso floppy${OSrev}.fs floppyB${OSrev}.fs \
	floppyC${OSrev}.fs miniroot${OSrev}.fs \
	pxeboot cdboot cdbr
