#	$OpenBSD: Makefile,v 1.2 2000/05/18 13:49:51 espie Exp $
#	From: Makefile,v 1.1 1999/05/05 07:37:06 wpaul Exp $

.if ${MACHINE} == "i386"
PROG=	wicontrol
SRCS=	wicontrol.c

CFLAGS+= -Wall
.else
NOPROG=yes
.endif

MAN=	wicontrol.8
MANSUBDIR=i386

.include <bsd.prog.mk>
