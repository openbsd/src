# $OpenBSD: Makefile,v 1.2 1999/07/13 23:02:05 deraadt Exp $

PROG=           ctm_dequeue
SRCS=           ctm_dequeue.c error.c
NOMAN=          yes
CFLAGS+=	-Wall -I${.CURDIR}/../ctm_rmail

.PATH: ${.CURDIR}/../ctm_rmail

.include <bsd.prog.mk>
