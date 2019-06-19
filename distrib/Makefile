#	$OpenBSD: Makefile,v 1.52 2019/05/11 07:18:14 deraadt Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armv7 arm64 hppa i386 \
	landisk loongson luna88k macppc \
	octeon sgi sparc64
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
