#	$OpenBSD: Makefile,v 1.36 2014/04/03 20:45:09 miod Exp $

#
# Be sure to change src/distrib/special/kbd/Makefile as well if you add or
# remove architectures on this list!
#
.if ${MACHINE} != "aviion" && ${MACHINE} != "hppa64" && \
    ${MACHINE} != "ia64" && ${MACHINE} != "octeon"
    
PROG=	kbd

SRCS=	main.c kbd_wscons.c
LDADD=	-lkvm
DPADD=	${LIBKVM}

.else
NOPROG=	Yes
.endif

MAN=		kbd.8
MANSUBDIR=	alpha amd64 armish armv7 hppa i386 landisk loongson luna88k \
		macppc sgi socppc sparc sparc64 vax zaurus

.include <bsd.prog.mk>
