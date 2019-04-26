#	$OpenBSD: Makefile.inc,v 1.27 2019/04/26 04:50:13 deraadt Exp $

KERNELS += GENERIC.MP bsd.mp

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}
	cp -p ${DESTDIR}/usr/mdec/bootxx ${RELEASEDIR}
	cp -p ${DESTDIR}/usr/mdec/netboot ${RELEASEDIR}
	cp -p ${DESTDIR}/usr/mdec/netboot.mop ${RELEASEDIR}

MDEXT=	bsd.rd cd${OSrev}.iso \
	miniroot${OSrev}.fs \
	boot bootxx netboot netboot.mop
