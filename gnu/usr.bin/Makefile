#	$OpenBSD: Makefile,v 1.7 1996/04/23 00:15:44 niklas Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

SUBDIR+=	bc cpio cvs dc diff diff3 gawk grep groff
SUBDIR+=	gzip rcs sdiff send-pr sort tar texinfo

# some ports don't use the tool chain in /usr/src
.if (${MACHINE_ARCH} != "alpha") && (${MACHINE_ARCH} != "mips")
SUBDIR+=	gas gdb ld
.endif
.if ${MACHINE_ARCH} != "alpha"
SUBDIR+=	gcc
.endif

.include <bsd.subdir.mk>
