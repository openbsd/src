#	$OpenBSD: Makefile,v 1.22 1998/03/11 21:42:18 maja Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

SUBDIR+=	bc cvs dc deroff diff diff3 gcc grep groff
SUBDIR+=	gzip lynx perl rcs sdiff sendbug sudo

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
