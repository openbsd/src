#	$OpenBSD: Makefile,v 1.27 2013/08/01 21:26:30 kettenis Exp $
#	$NetBSD: Makefile,v 1.1 1997/12/30 23:27:11 cgd Exp $

SUBDIR+= basename bc dc diff diff3 dirname file grep gzip gzsig
SUBDIR+= m4 mandoc sdiff sed sort tsort
SUBDIR+= xargs

.if defined(REGRESS_FULL)
SUBDIR+= cap_mkdb libtool make patch pkg-config rcs ssh
.endif

.include <bsd.subdir.mk>
