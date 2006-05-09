#	$OpenBSD: Makefile,v 1.26 2006/05/09 21:17:13 miod Exp $

.if ${MACHINE} != "aviion" && ${MACHINE} != "mvme68k" && \
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
MANSUBDIR=	alpha amd64 cats hp300 hppa i386 luna88k mac68k macppc \
		sgi sparc sparc64 zaurus

.include <bsd.prog.mk>
