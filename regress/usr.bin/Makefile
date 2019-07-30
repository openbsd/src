#	$OpenBSD: Makefile,v 1.43 2018/03/27 10:46:24 tobias Exp $

SUBDIR+= apply basename bc calendar colrm column cut dc diff diff3 dirname
SUBDIR+= doas file fmt fold grep gzip
SUBDIR+= jot lastcomm m4 mail mandoc openssl rev sdiff sed signify sort tsort
SUBDIR+= ul wc xargs

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= cap_mkdb libtool make patch pkg-config rcs ssh
.endif

.include <bsd.subdir.mk>
