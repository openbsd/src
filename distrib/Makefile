#	$OpenBSD: Makefile,v 1.45 2014/03/18 22:36:27 miod Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armish armv7 aviion hppa i386 \
	landisk loongson luna88k macppc \
	octeon sgi socppc sparc sparc64 vax zaurus
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
