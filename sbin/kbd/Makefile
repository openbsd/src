#	$OpenBSD: Makefile,v 1.24 2004/08/06 22:56:16 deraadt Exp $

#
# Be sure to change src/distrib/special/kbd/Makefile as well if you add
# architectures to this list!
#
.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "alpha") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "hppa") || (${MACHINE} == "sparc") || \
    (${MACHINE} == "sparc64") || (${MACHINE} == "vax") || \
    (${MACHINE} == "sgi") || (${MACHINE} == "cats")
    
PROG=	kbd

SRCS=	main.c kbd_wscons.c
LDADD=	-lkvm
DPADD=	${LIBKVM}

.else
NOPROG=	Yes
.endif

MAN=		kbd.8
MANSUBDIR=	alpha hppa i386 macppc sgi sparc sparc64 vax amd64 cats

.include <bsd.prog.mk>
