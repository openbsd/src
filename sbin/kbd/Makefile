#	$OpenBSD: Makefile,v 1.25 2005/01/13 19:17:25 drahn Exp $

#
# Be sure to change src/distrib/special/kbd/Makefile as well if you add
# architectures to this list!
#
.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "alpha") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "hppa") || (${MACHINE} == "sparc") || \
    (${MACHINE} == "sparc64") || (${MACHINE} == "vax") || \
    (${MACHINE} == "sgi") || (${MACHINE} == "cats") || \
    (${MACHINE} == "zaurus")
    
PROG=	kbd

SRCS=	main.c kbd_wscons.c
LDADD=	-lkvm
DPADD=	${LIBKVM}

.else
NOPROG=	Yes
.endif

MAN=		kbd.8
MANSUBDIR=	alpha hppa i386 macppc sgi sparc sparc64 vax amd64 cats zaurus

.include <bsd.prog.mk>
