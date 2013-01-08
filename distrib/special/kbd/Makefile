#	$OpenBSD: Makefile,v 1.17 2013/01/08 17:41:31 shadchin Exp $

.if ${MACHINE} != "aviion" && ${MACHINE} != "mvme68k" && \
    ${MACHINE} != "mvme88k"
PROG=	kbd

SRCS=	main.c kbd_wscons.c
CFLAGS+=-DNOKVM
.PATH:  ${.CURDIR}/../../../sbin/kbd
.else
NOPROG=	Yes
.endif

.include <bsd.prog.mk>
