#	$OpenBSD: Makefile,v 1.3 1999/02/01 05:37:27 todd Exp $

SUBDIR=	uucp

.if (${MACHINE_ARCH} == "mips") || make(clean) || make(cleandir) || make(obj)
SUBDIR+= ld.so
.endif

.include <bsd.subdir.mk>
