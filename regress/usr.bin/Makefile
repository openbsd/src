#	$OpenBSD: Makefile,v 1.28 2014/01/09 16:17:37 tedu Exp $
#	$NetBSD: Makefile,v 1.1 1997/12/30 23:27:11 cgd Exp $

SUBDIR+= basename bc dc diff diff3 dirname file grep gzip gzsig
SUBDIR+= m4 mandoc sdiff sed signify sort tsort
SUBDIR+= xargs

.if defined(REGRESS_FULL)
SUBDIR+= cap_mkdb libtool make patch pkg-config rcs ssh
.endif

.include <bsd.subdir.mk>
