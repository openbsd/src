#	$OpenBSD: Makefile,v 1.2 2002/04/01 18:26:04 beck Exp $

PROG=	authpf
MAN=	authpf.8
BINOWN= root
BINMODE= 4555
SRCS=	authpf.c parse.y pfctl_parser.c
CFLAGS+= -I${.CURDIR}/../../sbin/pfctl -Wall -Werror
.PATH:  ${.CURDIR}/../../sbin/pfctl

.include <bsd.prog.mk>
