#	$OpenBSD: Makefile,v 1.41 2017/06/08 17:33:21 bluhm Exp $

SUBDIR+= basename bc calendar colrm column cut dc diff diff3 dirname doas
SUBDIR+= file fmt fold grep gzip
SUBDIR+= jot lastcomm m4 mandoc openssl rev sdiff sed signify sort tsort ul
SUBDIR+= wc xargs

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= cap_mkdb libtool make patch pkg-config rcs ssh
.endif

.include <bsd.subdir.mk>
