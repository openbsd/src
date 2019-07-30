#	$OpenBSD: Makefile,v 1.51 2017/01/25 03:18:58 deraadt Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armv7 arm64 hppa i386 \
	landisk loongson luna88k macppc \
	octeon sgi socppc sparc64
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
