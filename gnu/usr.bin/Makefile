#	$OpenBSD: Makefile,v 1.42 2004/03/08 23:31:51 drahn Exp $
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

SUBDIR+=	cxxfilt cvs groff lynx rcs sendbug
.if ${MACHINE_ARCH} != "arm"
SUBDIR+=binutils
.endif

# most ports use binutils gas and ld, some do not (yet).
.if ${ELF_TOOLCHAIN:L} == "no"
SUBDIR+=	gas ld
.endif

# Do these last; texinfo builds the info 'dir' file, perl must be after binutils
SUBDIR+=	perl texinfo

.include <bsd.subdir.mk>
