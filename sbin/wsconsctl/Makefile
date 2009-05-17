#	$OpenBSD: Makefile,v 1.32 2009/05/17 01:55:13 deraadt Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "amd64" || \
   ${MACHINE} == "hp300" || ${MACHINE} == "hppa" || \
   ${MACHINE} == "i386" || ${MACHINE} == "luna88k" || \
   ${MACHINE} == "mac68k" || ${MACHINE} == "macppc" || \
   ${MACHINE} == "sgi" || ${MACHINE} == "sparc" || \
   ${MACHINE} == "sparc64" || ${MACHINE} == "vax" || \
   ${MACHINE} == "zaurus" || ${MACHINE} == "landisk" || \
   ${MACHINE} == "socppc"

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

.include <bsd.prog.mk>
