#	$OpenBSD: Makefile,v 1.11 2014/06/13 07:17:54 matthew Exp $

SUBDIR+= minherit_zero
SUBDIR+= misc mmap_4g mmap_size mmap_fixed mmap_mod mmap_write_self
SUBDIR+= mmap0

.if defined(REGRESS_FULL)
SUBDIR+= mmap_hint
.endif

install:

.include <bsd.subdir.mk>
