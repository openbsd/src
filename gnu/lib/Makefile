#	$OpenBSD: Makefile,v 1.18 2016/09/01 11:03:09 patrick Exp $

.include <bsd.own.mk>

SUBDIR+=libiberty libreadline
.if make(obj)
SUBDIR+=libobjc libstdc++ libstdc++-v3 libsupc++-v3 ../usr.bin/cc/libobjc
.else
.  if ${COMPILER_VERSION:L} == "gcc3"
SUBDIR+=libobjc libstdc++
.  else
# XXX make sure we build libobjc & libstdc++-v3 from gcc4
# XXX arm: libocj from gcc 4.2.1 does not build with ARM EHABI
.    if ${MACHINE_ARCH} != "arm"
SUBDIR+=../usr.bin/cc/libobjc
.    endif
SUBDIR+=libstdc++-v3 libsupc++-v3
.  endif
.endif

.include <bsd.subdir.mk>
