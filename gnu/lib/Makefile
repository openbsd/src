#	$OpenBSD: Makefile,v 1.15 2010/05/25 00:24:58 espie Exp $

.include <bsd.own.mk>

.if make(obj)
SUBDIR+=libiberty libobjc libstdc++ ../egcs libreadline \
libstdc++-v3 libsupc++-v3 ../usr.bin/cc/libobjc
.else
.  if ${COMPILER_VERSION:L} == "gcc3"
SUBDIR+=libiberty libobjc libstdc++ libreadline
.  elif ${COMPILER_VERSION:L} == "gcc4"
# XXX make sure we build libobjc & libstdc++-v3 from gcc4
SUBDIR+=libiberty libreadline ../usr.bin/cc/libobjc
SUBDIR+=libstdc++-v3 libsupc++-v3
.  else
SUBDIR+=libiberty libobjc ../egcs libreadline 
.  endif
.endif

.include <bsd.subdir.mk>
