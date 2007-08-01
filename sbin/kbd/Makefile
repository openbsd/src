#	$OpenBSD: Makefile,v 1.31 2007/08/01 21:34:01 deraadt Exp $

#
# Be sure to change src/distrib/special/kbd/Makefile as well if you add or
# remove architectures on this list!
#
.if ${MACHINE} != "aviion" && ${MACHINE} != "mvme68k" && \
    ${MACHINE} != "mvme88k" && ${MACHINE} != "landisk"
    
PROG=	kbd

SRCS=	main.c kbd_wscons.c
LDADD=	-lkvm
DPADD=	${LIBKVM}

.else
NOPROG=	Yes
.endif

MAN=		kbd.8
MANSUBDIR=	alpha amd64 hp300 hppa i386 luna88k mac68k macppc \
		sgi sparc sparc64 vax zaurus

.include <bsd.prog.mk>
