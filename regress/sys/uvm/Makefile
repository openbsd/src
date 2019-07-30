#	$OpenBSD: Makefile,v 1.13 2014/07/03 21:12:24 miod Exp $

SUBDIR+= minherit_zero
SUBDIR+= misc mmap_4g mmap_size mmap_fixed mmap_mod
SUBDIR+= mmap_noreplace mmap_write_self
SUBDIR+= mmap0

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= mmap_hint
.endif

install:

.include <bsd.subdir.mk>
