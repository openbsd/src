#	$OpenBSD: Makefile,v 1.30 2001/09/05 22:29:02 art Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

SUBDIR+=	bc cvs deroff diff diff3 
SUBDIR+=../egcs/gcc
#SUBDIR+= gcc 
SUBDIR+= grep groff gzip lynx rcs sdiff sendbug

SUBDIR+=	binutils
# some ports use binutils gas and ld, most do not (yet).
.if (${MACHINE_ARCH} != "alpha") && (${MACHINE_ARCH} != powerpc) && \
	(${MACHINE_ARCH} != "hppa") && (${MACHINE_ARCH} != "sparc64")
SUBDIR+=	gas ld
.endif

# Do these last; texinfo builds the info 'dir' file, perl must be after binutils
SUBDIR+=	perl texinfo

.include <bsd.subdir.mk>
