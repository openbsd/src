# $OpenBSD: Makefile,v 1.1 2017/03/17 14:45:16 rzalamena Exp $

.include <bsd.own.mk>

SRCS=	dhcrelay6.c dispatch.c log.c bpf.c packet.c
PROG=	dhcrelay6
MAN=	dhcrelay6.8

CFLAGS+=-Wall -Wextra -Werror
CFLAGS+=-Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations
CFLAGS+=-Wshadow -Wpointer-arith -Wsign-compare

.include <bsd.prog.mk>
