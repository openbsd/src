#	$OpenBSD: Makefile,v 1.52 2021/04/29 11:32:20 jsg Exp $
#	$NetBSD: Makefile,v 1.5 1995/09/15 21:05:21 pk Exp $

SUBDIR=	dev/microcode \
	arch/alpha arch/amd64 arch/arm64 arch/armv7 \
	arch/hppa arch/i386 \
	arch/landisk arch/loongson arch/luna88k \
	arch/macppc arch/octeon arch/powerpc64 \
	arch/riscv64 arch/sgi arch/sparc64

tags:
	cd ${.CURDIR}/kern; make tags

.include <bsd.subdir.mk>
