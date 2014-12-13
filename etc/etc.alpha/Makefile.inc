#	$OpenBSD: Makefile.inc,v 1.24 2014/12/13 14:45:57 miod Exp $

KERNELS += GENERIC.MP bsd.mp

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/bootxx ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/netboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/netboot.mop ${RELEASEDIR}

MDEXT=	bsd.rd cd${OSrev}.iso \
	floppy${OSrev}.fs floppyB${OSrev}.fs floppyC${OSrev}.fs \
	boot bootxx netboot netboot.mop
