#	$OpenBSD: Makefile,v 1.10 2004/01/23 18:21:13 espie Exp $

.include <bsd.own.mk>
USE_GCC3?=No

.if make(obj)
SUBDIR+=libiberty libobjc libstdc++ ../egcs libreadline
.else
.  if ${USE_GCC3:L} == "yes"
SUBDIR+=libiberty libobjc libstdc++ libreadline
.  else
SUBDIR+=libiberty ../egcs libreadline 
.  endif
.endif

.include <bsd.subdir.mk>
