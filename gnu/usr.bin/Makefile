#	$OpenBSD: Makefile,v 1.38 2003/09/26 07:35:03 otto Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

.include <bsd.own.mk>

SUBDIR+=	../egcs/gcc
SUBDIR+=	cvs groff lynx rcs sendbug binutils

# some ports use binutils gas and ld, most do not (yet).
.if ${ELF_TOOLCHAIN:L} == "no"
SUBDIR+=	gas ld
.endif

# Do these last; texinfo builds the info 'dir' file, perl must be after binutils
SUBDIR+=	perl texinfo

.include <bsd.subdir.mk>
