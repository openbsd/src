#	$OpenBSD: Makefile,v 1.40 2003/11/24 17:12:55 espie Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

.include <bsd.own.mk>

SUBDIR+=	../egcs/gcc
SUBDIR+=	cxxfilt cvs groff lynx rcs sendbug binutils

# most ports use binutils gas and ld, some do not (yet).
.if ${ELF_TOOLCHAIN:L} == "no"
SUBDIR+=	gas ld
.endif

# Do these last; texinfo builds the info 'dir' file, perl must be after binutils
SUBDIR+=	perl texinfo

.include <bsd.subdir.mk>
