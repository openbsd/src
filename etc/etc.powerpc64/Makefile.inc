#	$OpenBSD: Makefile.inc,v 1.3 2020/07/18 14:22:14 deraadt Exp $

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	boot bsd.rd miniroot${OSrev}.img
