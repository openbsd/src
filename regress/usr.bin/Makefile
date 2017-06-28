#	$OpenBSD: Makefile,v 1.42 2017/06/28 15:09:41 anton Exp $

SUBDIR+= basename bc calendar colrm column cut dc diff diff3 dirname doas
SUBDIR+= file fmt fold grep gzip
SUBDIR+= jot lastcomm m4 mail mandoc openssl rev sdiff sed signify sort tsort
SUBDIR+= ul wc xargs

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= cap_mkdb libtool make patch pkg-config rcs ssh
.endif

.include <bsd.subdir.mk>
