#	$OpenBSD: Makefile,v 1.38 2015/12/01 07:50:05 deraadt Exp $

#
# Be sure to change src/distrib/special/kbd/Makefile as well if you add or
# remove architectures on this list!
#
.if ${MACHINE} != "hppa64" && \
    ${MACHINE} != "ia64" && ${MACHINE} != "octeon"
    
PROG=	kbd

SRCS=	main.c kbd_wscons.c
LDADD=	-lkvm
DPADD=	${LIBKVM}

.else
NOPROG=	Yes
.endif

MAN=		kbd.8

.include <bsd.prog.mk>
