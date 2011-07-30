#	$OpenBSD: Makefile.inc,v 1.15 2011/07/30 05:38:41 miod Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	bsd.rd \
	bootsd bootst bootxx installboot netboot stboot tftpboot
