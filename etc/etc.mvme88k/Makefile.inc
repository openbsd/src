#	$OpenBSD: Makefile.inc,v 1.14 2011/07/22 09:30:54 espie Exp $

KERNELS += CONFIG.MP bsd.mp

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	bsd.rd \
	bootsd bootst bootxx installboot netboot stboot tftpboot
