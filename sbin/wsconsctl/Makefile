#	$OpenBSD: Makefile,v 1.17 2004/02/09 02:49:50 drahn Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "hppa" || ${MACHINE} == "macppc" || \
    ${MACHINE} == "sparc" || ${MACHINE} == "sparc64" || \
    ${MACHINE} == "pegasos" || ${MACHINE} == "alpha" || \
    ${MACHINE} == "cats"

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
MANSUBDIR=	alpha hppa i386 macppc sparc sparc64 pegasos amd64 cats

.include <bsd.prog.mk>
