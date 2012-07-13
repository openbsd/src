#	$OpenBSD: Makefile.inc,v 1.1.1.1 2012/07/13 17:49:53 eric Exp $

NOMAN=	noman

SRCS+=		common.c
CFLAGS+=	-Wall -Wstrict-prototypes -Werror -Wundef -g
LDFLAGS+=	-static
PROG:=		${PROG}${EXT}
