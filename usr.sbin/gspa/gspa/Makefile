#	$NetBSD: Makefile,v 1.1 1995/10/09 00:06:02 chopps Exp $

PROG=	gspa
CFLAGS+=-I. -I${.CURDIR}
HDRS=	gsp_ass.h gsp_code.h
SRCS=	gspa.c gsp_out.c gsp_sym.c gsp_lex.c gsp_act.c gsp_eval.c \
	gsp_inst.c gsp_pseu.c
OBJS+=	gsp_gram.o
NOMAN=	noman
CLEANFILES= y.tab.h

.include <bsd.prog.mk>
