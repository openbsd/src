# $OpenBSD: Makefile,v 1.2 1999/07/13 23:02:06 deraadt Exp $

PROG=           ctm_smail
SRCS=           ctm_smail.c error.c
NOMAN=          1
CFLAGS+=	-Wall -I${.CURDIR}/../ctm_rmail
.PATH: ${.CURDIR}/../ctm_rmail

.include <bsd.prog.mk>
