#	$OpenBSD: Makefile,v 1.6 1996/03/30 15:29:23 niklas Exp $
#	$NetBSD: Makefile,v 1.34 1996/02/18 12:28:15 pk Exp $

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
