#	$OpenBSD: Makefile,v 1.12 2014/06/19 19:34:22 matthew Exp $

SUBDIR+= minherit_zero
SUBDIR+= misc mmap_4g mmap_size mmap_fixed mmap_mod
SUBDIR+= mmap_noreplace mmap_write_self
SUBDIR+= mmap0

.if defined(REGRESS_FULL)
SUBDIR+= mmap_hint
.endif

install:

.include <bsd.subdir.mk>
