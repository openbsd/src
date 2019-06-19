#	$OpenBSD: Makefile,v 1.14 2019/01/16 21:46:54 bluhm Exp $

SUBDIR+= minherit_zero
SUBDIR+= mmap_4g mmap_size mmap_fixed mmap_mod
SUBDIR+= mmap_noreplace mmap_write_self
SUBDIR+= mmap0

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= mmap_hint
.endif

install:

.include <bsd.subdir.mk>
