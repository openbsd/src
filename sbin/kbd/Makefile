#	$OpenBSD: Makefile,v 1.35 2014/03/18 22:36:30 miod Exp $

#
# Be sure to change src/distrib/special/kbd/Makefile as well if you add or
# remove architectures on this list!
#
.if ${MACHINE} != "aviion"
    
PROG=	kbd

SRCS=	main.c kbd_wscons.c
LDADD=	-lkvm
DPADD=	${LIBKVM}

.else
NOPROG=	Yes
.endif

MAN=		kbd.8
MANSUBDIR=	alpha amd64 hppa i386 landisk luna88k macppc \
		sgi socppc sparc sparc64 vax zaurus

.include <bsd.prog.mk>
