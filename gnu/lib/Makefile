#	$OpenBSD: Makefile,v 1.11 2004/03/01 02:52:11 drahn Exp $

.include <bsd.own.mk>
USE_GCC3?=No

.if make(obj)
SUBDIR+=libiberty libobjc libstdc++ ../egcs libreadline libf2c
.else
.  if ${USE_GCC3:L} == "yes"
SUBDIR+=libiberty libobjc libstdc++ libreadline libf2c
.  else
SUBDIR+=libiberty ../egcs libreadline 
.  endif
.endif

.include <bsd.subdir.mk>
