#	$OpenBSD: Makefile,v 1.3 1996/06/23 14:32:24 deraadt Exp $
#	$NetBSD: Makefile,v 1.14 1995/06/20 22:25:51 christos Exp $

# disable an undesirable feature
#CFLAGS=-DTRACING

PROG=	routed
SRCS=	af.c if.c input.c main.c output.c startup.c tables.c timer.c \
	trace.c inet.c
MAN=	routed.8
#SUBDIR=	query trace

.include <bsd.prog.mk>
