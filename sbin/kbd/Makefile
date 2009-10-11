#	$OpenBSD: Makefile,v 1.33 2009/10/11 12:19:49 maja Exp $

#
# Be sure to change src/distrib/special/kbd/Makefile as well if you add or
# remove architectures on this list!
#
.if ${MACHINE} != "aviion" && ${MACHINE} != "mvme68k" && \
    ${MACHINE} != "mvme88k"
    
PROG=	kbd

SRCS=	main.c kbd_wscons.c
LDADD=	-lkvm
DPADD=	${LIBKVM}

.else
NOPROG=	Yes
.endif

MAN=		kbd.8
MANSUBDIR=	alpha amd64 hp300 hppa i386 landisk luna88k mac68k macppc \
		sgi socppc sparc sparc64 vax zaurus

.include <bsd.prog.mk>
