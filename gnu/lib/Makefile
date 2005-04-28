#	$OpenBSD: Makefile,v 1.12 2005/04/28 21:30:13 espie Exp $

.include <bsd.own.mk>
USE_GCC3?=No

.if make(obj)
SUBDIR+=libiberty libobjc libstdc++ ../egcs libreadline libf2c
.else
.  if ${USE_GCC3:L} == "yes"
SUBDIR+=libiberty libobjc libstdc++ libreadline libf2c
.  else
SUBDIR+=libiberty libobjc ../egcs libreadline 
.  endif
.endif

.include <bsd.subdir.mk>
