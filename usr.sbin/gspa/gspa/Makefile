#	$NetBSD: Makefile,v 1.3 1996/02/06 18:30:58 jtc Exp $

PROG=	gspa
CFLAGS+=-I. -I${.CURDIR}
HDRS=	gsp_ass.h gsp_code.h
SRCS=	gsp_gram.c gspa.c gsp_out.c gsp_sym.c gsp_lex.c gsp_act.c \
	gsp_eval.c gsp_inst.c gsp_pseu.c
CLEANFILES+=gsp_gram.c y.tab.h

gsp_eval.o gsp_lex.o gspa.o: gsp_gram.c

.include <bsd.prog.mk>
