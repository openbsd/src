#	$OpenBSD: Makefile,v 1.2 2001/07/05 02:14:38 mickey Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "powerpc") || \
    (${MACHINE} == "alpha")
PROG=   iopctl
.else
NOPROG=
.endif

MAN=	iopctl.8

.include <bsd.prog.mk>
