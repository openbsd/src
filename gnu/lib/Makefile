#	$OpenBSD: Makefile,v 1.21 2017/04/17 15:53:21 kettenis Exp $

.include <bsd.own.mk>

SUBDIR+=libiberty libreadline
.if make(obj)
SUBDIR+=libobjc libstdc++ libstdc++-v3 libsupc++-v3 ../usr.bin/cc/libobjc
.else
.  if ${BUILD_GCC3:L} == "yes"
SUBDIR+=libobjc libstdc++
.  endif
.  if ${BUILD_GCC4:L} == "yes"
# XXX make sure we build libobjc & libstdc++-v3 from gcc4
SUBDIR+=../usr.bin/cc/libobjc
SUBDIR+=libstdc++-v3 libsupc++-v3
.  endif
.endif

.include <bsd.subdir.mk>
