#	$OpenBSD: Makefile,v 1.34 2003/07/22 17:15:12 brad Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

.include <bsd.own.mk>

SUBDIR+=	bc cvs diff diff3 
SUBDIR+=../egcs/gcc
#SUBDIR+= gcc 
SUBDIR+= groff gzip lynx rcs sdiff sendbug

SUBDIR+=	binutils
# some ports use binutils gas and ld, most do not (yet).
.if ${ELF_TOOLCHAIN:L} == "no"
SUBDIR+=	gas ld
.endif

# Do these last; texinfo builds the info 'dir' file, perl must be after binutils
SUBDIR+=	perl texinfo

.include <bsd.subdir.mk>
