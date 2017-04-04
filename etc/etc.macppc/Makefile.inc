#	$OpenBSD: Makefile.inc,v 1.17 2017/04/04 00:03:12 deraadt Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/ofwboot ${RELEASEDIR}/ofwboot
	cp -p ${DESTDIR}/usr/mdec/bsd.tbxi ${RELEASEDIR}/bsd.tbxi
	cp -p ${DESTDIR}/usr/mdec/boot.mac ${RELEASEDIR}/boot.mac

MDEXT=	bsd.rd cd${OSrev}.iso ofwboot bsd.tbxi boot.mac
