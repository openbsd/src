#	$OpenBSD: Makefile,v 1.45 2007/03/23 02:17:52 ray Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

.include <bsd.own.mk>
USE_GCC3?=No

.if make(obj)
SUBDIR+=	gcc ../egcs/gcc
.else
.  if ${USE_GCC3:L} == "yes"
SUBDIR+=	gcc
.  else
SUBDIR+=	../egcs/gcc
.  endif
.endif

SUBDIR+=	cxxfilt cvs groff lynx binutils

# most ports use binutils gas and ld, some do not (yet).
.if ${ELF_TOOLCHAIN:L} == "no"
SUBDIR+=	gas ld
.endif

# Do these last; texinfo builds the info 'dir' file, perl must be after binutils
SUBDIR+=	perl texinfo

.include <bsd.subdir.mk>
