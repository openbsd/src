# $OpenBSD: Makefile,v 1.1 2016/07/19 16:54:26 reyk Exp $

NAME=		switch

.PATH:		${.CURDIR}/../${NAME}d

PROG=		${NAME}ctl
MAN=		${NAME}ctl.8
SRCS=		log.c ${NAME}ctl.c parser.c util.c

LDADD=		-lutil
DPADD=		${LIBUTIL}

CFLAGS+=	-DOFD_NAME=\"${NAME}\"
CFLAGS+=	-Wall -I${.CURDIR} -I${.CURDIR}/../${NAME}d
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare

.include <bsd.prog.mk>
