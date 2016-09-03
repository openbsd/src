#	$OpenBSD: Makefile,v 1.48 2016/09/03 13:37:42 guenther Exp $
#	$NetBSD: Makefile,v 1.5 1995/09/15 21:05:21 pk Exp $

SUBDIR=	dev/microcode \
	arch/alpha arch/amd64 arch/armv7 \
	arch/hppa arch/i386 \
	arch/landisk arch/loongson arch/luna88k \
	arch/macppc arch/octeon \
	arch/sgi arch/socppc arch/sparc64

tags:
	cd ${.CURDIR}/kern; make tags

.include <bsd.subdir.mk>
