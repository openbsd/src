#	$OpenBSD: Makefile,v 1.25 2021/06/09 19:44:55 mortimer Exp $

.include <bsd.own.mk>

SUBDIR+=libcompiler_rt libexecinfo libiberty libreadline
SUBDIR+=libcxxabi libcxx
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
