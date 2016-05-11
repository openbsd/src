#	$OpenBSD: Makefile,v 1.45 2016/05/11 21:52:49 deraadt Exp $
#	$NetBSD: Makefile,v 1.5 1995/09/15 21:05:21 pk Exp $

SUBDIR=	dev/microcode \
	arch/alpha arch/amd64 arch/armish arch/armv7 \
	arch/hppa arch/i386 \
	arch/landisk arch/loongson arch/luna88k \
	arch/macppc arch/octeon \
	arch/sgi arch/socppc arch/sparc arch/sparc64 \
	arch/zaurus

tags:
	cd ${.CURDIR}/kern; make tags

.include <bsd.subdir.mk>
