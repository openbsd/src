#	$OpenBSD: Makefile,v 1.27 2025/10/23 19:06:09 miod Exp $

.include <bsd.own.mk>

SUBDIR+=libclang_rt libcompiler_rt libexecinfo libiberty libreadline
SUBDIR+=libcxxabi libcxx
.if make(obj)
SUBDIR+=libstdc++-v3 libsupc++-v3
.else
.  if ${BUILD_GCC4:L} == "yes"
SUBDIR+=libstdc++-v3 libsupc++-v3
.  endif
.endif

.include <bsd.subdir.mk>
