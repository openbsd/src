#	$OpenBSD: Makefile,v 1.8 2001/09/01 16:21:16 drahn Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "alpha" || ${MACHINE} == "hppa" || \
    ${MACHINE} == "macppc"

PROG=	wsconsctl
SRCS=	display.c keyboard.c keysym.c map_parse.y map_scan.l \
	mouse.c util.c wsconsctl.c

CPPFLAGS+=	-I.
CLEANFILES+=	keysym.h y.tab.h

beforedepend: keysym.h

keysym.h: mkkeysym.sh ${DESTDIR}/usr/include/dev/wscons/wsksymdef.h
	sh ${.CURDIR}/mkkeysym.sh \
	  ${DESTDIR}/usr/include/dev/wscons/wsksymdef.h > keysym.h
.else

NOPROG=yes

.endif

MAN=	wsconsctl.8
MANSUBDIR=	alpha hppa i386 macppc

.include <bsd.prog.mk>
