#	$OpenBSD: Makefile,v 1.28 1999/11/18 16:36:50 millert Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

SUBDIR+=	bc cvs deroff diff diff3 
SUBDIR+=../egcs/gcc
#SUBDIR+= gcc 
SUBDIR+= grep groff gzip lynx rcs sdiff sendbug

SUBDIR+=	binutils
# some ports use binutils gas and ld, most do not (yet).
.if (${MACHINE_ARCH} != "alpha") && (${MACHINE_ARCH} != "mips") && \
	(${MACHINE_ARCH} != "powerpc") && (${MACHINE_ARCH} != "hppa")
SUBDIR+=	gas ld
.endif

# Do these last; texinfo builds the info 'dir' file, perl must be after binutils
SUBDIR+=	perl texinfo

.include <bsd.subdir.mk>
