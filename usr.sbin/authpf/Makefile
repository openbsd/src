#	$OpenBSD: Makefile,v 1.1 2002/04/01 17:43:42 beck Exp $

PROG=	authpf
MAN=	authpf.8
SRCS=	authpf.c parse.y pfctl_parser.c
CFLAGS+= -I${.CURDIR}/../../sbin/pfctl -Wall -Werror
.PATH:  ${.CURDIR}/../../sbin/pfctl

.include <bsd.prog.mk>
