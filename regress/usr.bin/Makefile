#	$OpenBSD: Makefile,v 1.44 2018/10/15 08:08:46 otto Exp $

SUBDIR+= apply basename bc calendar colrm column cut dc diff diff3 dirname
SUBDIR+= doas file fmt fold grep gzip join
SUBDIR+= jot lastcomm m4 mail mandoc openssl rev sdiff sed signify sort tsort
SUBDIR+= ul wc xargs

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= cap_mkdb libtool make patch pkg-config rcs ssh
.endif

.include <bsd.subdir.mk>
