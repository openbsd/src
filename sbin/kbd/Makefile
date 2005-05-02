#	$OpenBSD: Makefile,v 1.26 2005/05/02 23:14:34 martin Exp $

#
# Be sure to change src/distrib/special/kbd/Makefile as well if you add or
# remove architectures on this list!
#
.if ${MACHINE} != "mac68k" && ${MACHINE} != "mvme68k" && \
    ${MACHINE} != "mvme88k"
    
PROG=	kbd

SRCS=	main.c kbd_wscons.c
LDADD=	-lkvm
DPADD=	${LIBKVM}

.else
NOPROG=	Yes
.endif

MAN=		kbd.8
MANSUBDIR=	alpha hppa i386 macppc sgi sparc sparc64 vax amd64 cats zaurus
MANSUBDIR=	alpha amd64 cats hp300 hppa i386 macppc sgi sparc sparc64 vax \
		zaurus

.include <bsd.prog.mk>
