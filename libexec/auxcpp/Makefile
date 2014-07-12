# $OpenBSD: Makefile,v 1.2 2014/07/12 16:27:49 matthieu Exp $

PROG=	auxcpp
MAN=	auxcpp.1

CPPFLAGS += -DUCPP_CONFIG -DSTAND_ALONE

SRCS= assert.c cpp.c eval.c lexer.c macro.c mem.c nhash.c

.include <bsd.prog.mk>
