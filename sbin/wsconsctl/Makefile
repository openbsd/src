#	$OpenBSD: Makefile,v 1.23 2005/03/29 22:35:22 mickey Exp $

.if ${MACHINE} != "mac68k" && ${MACHINE} != "mvme68k" && ${MACHINE} != "mvme88k"

PROG=	wsconsctl
SRCS=	display.c keyboard.c keysym.c map_parse.y map_scan.l \
	mouse.c util.c wsconsctl.c

CPPFLAGS+=	-I${.CURDIR} -I.
CLEANFILES+=	keysym.h y.tab.h

beforedepend: keysym.h

keysym.h: mkkeysym.sh ${DESTDIR}/usr/include/dev/wscons/wsksymdef.h
	sh ${.CURDIR}/mkkeysym.sh \
	  ${DESTDIR}/usr/include/dev/wscons/wsksymdef.h > keysym.h
.else

NOPROG=yes

.endif

MAN=	wsconsctl.8
MANSUBDIR=	alpha amd64 cats hp300 hppa i386 luna88k macppc sgi
MANSUBDIR+=	sparc sparc64 zaurus

.include <bsd.prog.mk>
