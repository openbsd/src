#	$OpenBSD: Makefile,v 1.17 2014/11/10 19:06:47 guenther Exp $

.if make(obj)
SUBDIR= alpha amd64 arm hppa i386 ia64 m88k mips64 powerpc sh sparc sparc64 vax
.else
SUBDIR= ${MACHINE_CPU}
.endif

.include <bsd.subdir.mk>
