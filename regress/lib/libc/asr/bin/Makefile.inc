#	$OpenBSD: Makefile.inc,v 1.3 2026/05/08 04:02:24 tb Exp $

NOMAN=	noman

SRCS+=		common.c
CFLAGS+=	-Wall -Wstrict-prototypes -Werror -Wundef -g
CFLAGS+=	-Wno-unused-but-set-variable
LDSTATIC=	${STATIC}
PROG:=		${PROG}${EXT}
