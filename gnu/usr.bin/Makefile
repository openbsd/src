#	$OpenBSD: Makefile,v 1.14 1996/08/19 10:48:58 downsj Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

SUBDIR+=	bc cpio cvs dc diff diff3 gcc grep groff
SUBDIR+=	gzip perl rcs sdiff sendbug sort

# some ports use binutils, some do not.
.if (${MACHINE_ARCH} == "alpha") || (${MACHINE_ARCH} == "mips")
SUBDIR+=	binutils
.else
SUBDIR+=	gas gdb ld
.endif

# Do this last, so that it re-builds the info 'dir' file
SUBDIR+= texinfo

.include <bsd.subdir.mk>
