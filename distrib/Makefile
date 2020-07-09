#	$OpenBSD: Makefile,v 1.53 2020/07/04 21:11:03 kettenis Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armv7 arm64 hppa i386 \
	landisk loongson luna88k macppc \
	octeon powerpc64 sgi sparc64
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
