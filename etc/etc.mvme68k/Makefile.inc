#	$OpenBSD: Makefile.inc,v 1.14 2011/07/22 09:30:54 espie Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	bsd.rd \
	installboot netboot rboot sboot stboot bootsd bootst bootxx
