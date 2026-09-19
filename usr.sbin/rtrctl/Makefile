#	$OpenBSD: Makefile,v 1.3 2026/09/19 17:13:06 deraadt Exp $

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
CFLAGS+= -Wextra

.include <bsd.prog.mk>
