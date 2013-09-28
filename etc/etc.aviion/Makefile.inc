#	$OpenBSD: Makefile.inc,v 1.7 2013/09/28 21:34:10 miod Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	# bsd.rd	# bootblocks?
