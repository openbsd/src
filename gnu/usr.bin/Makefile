#	$OpenBSD: Makefile,v 1.15 1996/09/14 06:00:40 etheisen Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

SUBDIR+=	bc cpio cvs dc diff diff3 gcc grep 
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
