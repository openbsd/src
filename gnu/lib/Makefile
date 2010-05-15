#	$OpenBSD: Makefile,v 1.14 2010/05/15 08:28:18 robert Exp $

.include <bsd.own.mk>

.if make(obj)
SUBDIR+=libiberty libobjc libstdc++ ../egcs libreadline libf2c \
libstdc++-v3 libsupc++-v3 ../usr.bin/cc/libobjc
.else
.  if ${COMPILER_VERSION:L} == "gcc3"
SUBDIR+=libiberty libobjc libstdc++ libreadline libf2c
.  elif ${COMPILER_VERSION:L} == "gcc4"
# XXX make sure we build libobjc & libstdc++-v3 from gcc4
SUBDIR+=libiberty libreadline ../usr.bin/cc/libobjc
SUBDIR+=libstdc++-v3 libsupc++-v3
.  else
SUBDIR+=libiberty libobjc ../egcs libreadline 
.  endif
.endif

.include <bsd.subdir.mk>
