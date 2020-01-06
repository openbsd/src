#	$OpenBSD: Makefile,v 1.45 2020/01/06 23:01:16 bluhm Exp $

SUBDIR+= apply
SUBDIR+= basename bc
SUBDIR+= calendar colrm column cut
SUBDIR+= dc diff diff3 dirname doas
SUBDIR+= file fmt fold
SUBDIR+= grep gzip
SUBDIR+= join jot
SUBDIR+= lastcomm
SUBDIR+= m4 mail mandoc
SUBDIR+= nc
SUBDIR+= openssl
SUBDIR+= rev
SUBDIR+= sdiff sed signify sort
SUBDIR+= tsort
SUBDIR+= ul
SUBDIR+= wc
SUBDIR+= xargs

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= cap_mkdb libtool make patch pkg-config rcs ssh
.endif

.include <bsd.subdir.mk>
