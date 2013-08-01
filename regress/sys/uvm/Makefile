#	$OpenBSD: Makefile,v 1.10 2013/08/01 21:26:30 kettenis Exp $

SUBDIR+= misc mmap_4g mmap_size mmap_fixed mmap_mod mmap_write_self
SUBDIR+= mmap0

.if defined(REGRESS_FULL)
SUBDIR+= mmap_hint
.endif

install:

.include <bsd.subdir.mk>
