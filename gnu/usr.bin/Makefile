#	$OpenBSD: Makefile,v 1.23 1998/03/29 22:17:15 niklas Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

SUBDIR+=	bc cvs dc deroff diff diff3 gcc grep groff
SUBDIR+=	gzip lynx perl rcs sdiff sendbug sudo

SUBDIR+=	binutils
# some ports use binutils gas and ld, most do not (yet).
.if (${MACHINE_ARCH} != "alpha") && (${MACHINE_ARCH} != "mips") && \
	(${MACHINE_ARCH} != "powerpc")
SUBDIR+=	gas ld
.endif

# Do this last, so that it re-builds the info 'dir' file
SUBDIR+= texinfo

.include <bsd.subdir.mk>
