#	$OpenBSD: Makefile,v 1.39 2016/10/07 15:31:42 schwarze Exp $

SUBDIR+= basename bc calendar colrm column dc diff diff3 dirname doas
SUBDIR+= file fmt fold grep gzip
SUBDIR+= jot m4 mandoc openssl rev sdiff sed signify sort tsort ul
SUBDIR+= xargs

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= cap_mkdb libtool make patch pkg-config rcs ssh
.endif

.include <bsd.subdir.mk>
