#	$NetBSD: Makefile,v 1.2 1995/11/17 10:05:23 thorpej Exp $

PROG=	gspa
CFLAGS+=-I. -I${.CURDIR}
HDRS=	gsp_ass.h gsp_code.h
SRCS=	gspa.c gsp_out.c gsp_sym.c gsp_lex.c gsp_act.c gsp_eval.c \
	gsp_inst.c gsp_pseu.c gsp_gram.c
NOMAN=	noman
CLEANFILES+=gsp_gram.c y.tab.h

.include <bsd.prog.mk>
