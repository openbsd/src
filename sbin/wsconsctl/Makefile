#	$OpenBSD: Makefile,v 1.24 2005/04/04 04:25:42 mickey Exp $

.if ${MACHINE} != "mac68k" && ${MACHINE} != "mvme68k" && \
    ${MACHINE} != "mvme88k" && ${MACHINE} != "vax"

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
