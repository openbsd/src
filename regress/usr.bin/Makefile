#	$OpenBSD: Makefile,v 1.30 2014/08/26 17:50:07 jsing Exp $
#	$NetBSD: Makefile,v 1.1 1997/12/30 23:27:11 cgd Exp $

SUBDIR+= basename bc dc diff diff3 dirname file grep gzip gzsig
SUBDIR+= m4 mandoc openssl sdiff sed signify sort tsort
SUBDIR+= xargs

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= cap_mkdb libtool make patch pkg-config rcs ssh
.endif

.include <bsd.subdir.mk>
