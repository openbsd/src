#	$OpenBSD: Makefile,v 1.1 2004/01/15 21:32:03 millert Exp $

.PATH:		${.CURDIR}/..

LIB=	sudo
SRCS=	sudo.tab.c lex.yy.c alloc.c defaults.c fileops.c goodpath.c find_path.c
CPPFLAGS+=	-I.

DEBUGLIBS= no
NOPROFILE= yes
NOPIC=	yes

sudo.tab.c sudo.tab.h: parse.yacc
	rm -f sudo.tab.h sudo.tab.c
	${YACC} -d -b sudo ${.CURDIR}/../parse.yacc

lex.yy.c: parse.lex
	rm -f lex.yy.c
	${LEX} ${.CURDIR}/../parse.lex

install:
	@echo -n

.include <bsd.lib.mk>
