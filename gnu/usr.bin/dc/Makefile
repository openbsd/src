#	$NetBSD: Makefile,v 1.2 1995/04/23 07:58:29 cgd Exp $

PROG=	dc
CFLAGS+=-D_POSIX_SOURCE -I${.CURDIR} -I${.CURDIR}/../bc

SRCS= array.c dc-number.c number.c eval.c misc.c stack.c string.c

.PATH: ${.CURDIR}/../bc

.include <bsd.prog.mk>
