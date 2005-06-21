#	$OpenBSD: Makefile,v 1.3 2005/06/21 02:11:43 brad Exp $

.PATH:		${.CURDIR}/..

LIB=	sudo
SRCS=	alloc.c defaults.c fileops.c find_path.c gettime.c goodpath.c \
	lex.yy.c sudo.tab.c
CPPFLAGS+=	-I.

CLEANFILES=	lex.yy.c sudo.tab.c sudo.tab.h

DEBUGLIBS= no
NOPROFILE= yes

.include <bsd.own.mk>
.if !defined(NOPIC)
CPPFLAGS+= -D_PATH_SUDO_NOEXEC=\"/usr/libexec/sudo_noexec.so\"
NOPIC=	yes
.endif

sudo.tab.c sudo.tab.h: parse.yacc
	rm -f sudo.tab.h sudo.tab.c
	${YACC} -d -b sudo ${.CURDIR}/../parse.yacc

lex.yy.c: parse.lex
	rm -f lex.yy.c
	${LEX} ${.CURDIR}/../parse.lex

install:
	@echo -n

.include <bsd.lib.mk>
