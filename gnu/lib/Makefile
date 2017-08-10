#	$OpenBSD: Makefile,v 1.22 2017/08/10 16:06:59 mortimer Exp $

.include <bsd.own.mk>

SUBDIR+=libiberty libreadline
.if make(obj)
SUBDIR+=libstdc++ libstdc++-v3 libsupc++-v3
.else
.  if ${BUILD_GCC3:L} == "yes"
SUBDIR+=libstdc++
.  endif
.  if ${BUILD_GCC4:L} == "yes"
# XXX make sure we build libstdc++-v3 from gcc4
SUBDIR+=libstdc++-v3 libsupc++-v3
.  endif
.endif

.include <bsd.subdir.mk>
