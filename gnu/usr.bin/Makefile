#	$OpenBSD: Makefile,v 1.25 1998/06/29 00:09:51 millert Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

SUBDIR+=	bc cvs deroff diff diff3 gcc grep groff
SUBDIR+=	gzip lynx perl rcs sdiff sendbug sudo

SUBDIR+=	binutils
# some ports use binutils gas and ld, most do not (yet).
.if (${MACHINE_ARCH} != "alpha") && (${MACHINE_ARCH} != "mips") && \
	(${MACHINE_ARCH} != "powerpc") && (${MACHINE_ARCH} != "hppa")
SUBDIR+=	gas ld
.endif

# Do this last, so that it re-builds the info 'dir' file
SUBDIR+= texinfo

.include <bsd.subdir.mk>
