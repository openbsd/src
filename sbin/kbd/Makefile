#	$OpenBSD: Makefile,v 1.19 2004/01/29 02:55:48 deraadt Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "macppc") || (${MACHINE} == "pegasos") || \
    (${MACHINE} == "alpha") || (${MACHINE} == "hppa") || \
    (${MACHINE} == "sparc") || (${MACHINE} == "sparc64") || \
    (${MACHINE} == "vax") || (${MACHINE} == "cats")
    
PROG=	kbd

SRCS=	main.c kbd_wscons.c
LDADD=	-lkvm
DPADD=	${LIBKVM}

.else
NOPROG=	Yes
.endif

MAN=		kbd.8
MANSUBDIR=	alpha hppa i386 macppc sparc sparc64 vax pegosos amd64 cats

.include <bsd.prog.mk>
