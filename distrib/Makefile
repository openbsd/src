#	$OpenBSD: Makefile,v 1.50 2016/09/03 13:37:39 guenther Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armv7 hppa i386 \
	landisk loongson luna88k macppc \
	octeon sgi socppc sparc64
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
