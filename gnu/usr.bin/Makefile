#	$OpenBSD: Makefile,v 1.8 1996/06/27 21:11:33 tholo Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

SUBDIR+=	bc cpio cvs dc diff diff3 gawk grep groff
SUBDIR+=	gzip rcs sdiff sendbug sort tar texinfo

# some ports don't use the tool chain in /usr/src
.if (${MACHINE_ARCH} != "alpha") && (${MACHINE_ARCH} != "mips")
SUBDIR+=	gas gdb ld
.endif
.if ${MACHINE_ARCH} != "alpha"
SUBDIR+=	gcc
.endif

.include <bsd.subdir.mk>
