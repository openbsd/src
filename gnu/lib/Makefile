#	$OpenBSD: Makefile,v 1.19 2016/09/01 15:53:21 kettenis Exp $

.include <bsd.own.mk>

SUBDIR+=libiberty libreadline
.if make(obj)
SUBDIR+=libobjc libstdc++ libstdc++-v3 libsupc++-v3 ../usr.bin/cc/libobjc
.else
.  if ${COMPILER_VERSION:L} == "gcc3"
SUBDIR+=libobjc libstdc++
.  else
# XXX make sure we build libobjc & libstdc++-v3 from gcc4
SUBDIR+=../usr.bin/cc/libobjc
SUBDIR+=libstdc++-v3 libsupc++-v3
.  endif
.endif

.include <bsd.subdir.mk>
