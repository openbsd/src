#	$OpenBSD: Makefile.inc,v 1.16 2013/05/12 12:15:59 miod Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	bsd.rd \
	bootsd bootst bootxx installboot netboot sboot stboot tftpboot
