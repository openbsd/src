#	$OpenBSD: Makefile,v 1.2 2016/09/01 10:08:29 deraadt Exp $

.PATH: ${.CURDIR}/../../../usr.sbin/eeprom

.if ${MACHINE} == "macppc" || ${MACHINE} == "sparc64"
PROG=	eeprom

SRCS=	main.c
SRCS+=	ophandlers.c optree.c

CFLAGS+=	-DSMALL
.else
NOPROG=yes
.endif

.include <bsd.prog.mk>
