#	$OpenBSD: Makefile,v 1.18 2014/03/18 22:36:29 miod Exp $

.if ${MACHINE} != "aviion"
PROG=	kbd

SRCS=	main.c kbd_wscons.c
CFLAGS+=-DNOKVM
.PATH:  ${.CURDIR}/../../../sbin/kbd
.else
NOPROG=	Yes
.endif

.include <bsd.prog.mk>
