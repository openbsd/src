#	$OpenBSD: Makefile,v 1.2 2026/09/16 16:52:41 deraadt Exp $

.PATH:		${.CURDIR}/../rtrd

PROG=	rtrctl

MAN=	rtrctl.8

SRCS=	rtrctl.c ometric.c

CFLAGS+= -Wall -I${.CURDIR} -I${.CURDIR}/../rtrd
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -Werror=implicit
CFLAGS+= -Wextra -Wpedantic

.include <bsd.prog.mk>
