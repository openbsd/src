#	$OpenBSD: Makefile,v 1.28 2006/06/06 05:07:16 deraadt Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
   ${MACHINE} == "cats" || ${MACHINE} == "hp300" || \
   ${MACHINE} == "hppa" || ${MACHINE} == "i386" || \
   ${MACHINE} == "luna88k" || ${MACHINE} == "mac68k" || \
   ${MACHINE} == "macppc" || ${MACHINE} == "sgi" || \
   ${MACHINE} == "sparc" || ${MACHINE} == "sparc64" || \
   ${MACHINE} == "zaurus"

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
