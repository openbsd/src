#	$OpenBSD: Makefile,v 1.1 2026/09/16 16:11:46 job Exp $

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
CFLAGS+= -MD -MP
CFLAGS+= -O2 -pipe

LDADD= -lc

.include <bsd.prog.mk>
