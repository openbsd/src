#	$OpenBSD: Makefile,v 1.8 2003/01/03 21:37:44 cedric Exp $

PROG=	authpf
MAN=	authpf.8
BINOWN= root
BINGRP= authpf
BINMODE= 6555
SRCS=	authpf.c parse.y pfctl_parser.c pf_print_state.c pfctl_altq.c
SRCS+=	pfctl_radix.c
CFLAGS+= -I${.CURDIR}/../../sbin/pfctl -Wall -Werror
.PATH:  ${.CURDIR}/../../sbin/pfctl

LDADD+= -lm
DPADD+= ${LIBM}

CLEANFILES+=y.tab.h

.include <bsd.prog.mk>
