#	$OpenBSD: Makefile,v 1.5 2002/06/07 17:07:48 beck Exp $

PROG=	authpf
MAN=	authpf.8
BINOWN= root
BINGRP= authpf
BINMODE= 6555
SRCS=	authpf.c parse.y pfctl_parser.c pf_print_state.c
CFLAGS+= -I${.CURDIR}/../../sbin/pfctl -Wall -Werror
.PATH:  ${.CURDIR}/../../sbin/pfctl

.include <bsd.prog.mk>
