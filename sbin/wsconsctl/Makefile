#	$OpenBSD: Makefile,v 1.1 2000/07/01 23:52:45 mickey Exp $
#	$NetBSD: Makefile,v 1.1 1998/12/28 14:01:16 hannken Exp $

PROG=	wsconsctl
SRCS=	display.c keyboard.c keysym.c map_parse.y map_scan.l \
	mouse.c util.c wsconsctl.c
MAN=	wsconsctl.8

CPPFLAGS+=	-I.
CLEANFILES+=	keysym.h y.tab.h

beforedepend: keysym.h

keysym.h: mkkeysym.sh ${DESTDIR}/usr/include/dev/wscons/wsksymdef.h
	sh ${.CURDIR}/mkkeysym.sh \
	  ${DESTDIR}/usr/include/dev/wscons/wsksymdef.h > keysym.h

.include <bsd.prog.mk>
