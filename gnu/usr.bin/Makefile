#	$OpenBSD: Makefile,v 1.19 1997/01/21 04:34:33 millert Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

SUBDIR+=	bc cpio cvs dc diff diff3 gcc grep groff
SUBDIR+=	gzip perl rcs sdiff sendbug sudo

# some ports use binutils, some do not.
.if (${MACHINE_ARCH} == "alpha") || (${MACHINE_ARCH} == "mips") || \
	(${MACHINE_ARCH} == "powerpc")
SUBDIR+=	binutils
.else
SUBDIR+=	gas gdb ld
.endif

# Do this last, so that it re-builds the info 'dir' file
SUBDIR+= texinfo

.include <bsd.subdir.mk>
