#	$OpenBSD: Makefile,v 1.24 1998/05/04 21:35:29 mickey Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

SUBDIR+=	bc cvs dc deroff diff diff3 gcc grep groff
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
