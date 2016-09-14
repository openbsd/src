#	$OpenBSD: Makefile,v 1.37 2016/09/14 16:04:34 millert Exp $
#	$NetBSD: Makefile,v 1.1 1997/12/30 23:27:11 cgd Exp $

SUBDIR+= basename bc calendar dc diff diff3 dirname doas file fmt fold grep gzip
SUBDIR+= jot m4 mandoc openssl rev sdiff sed signify sort tsort
SUBDIR+= xargs

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= cap_mkdb libtool make patch pkg-config rcs ssh
.endif

.include <bsd.subdir.mk>
