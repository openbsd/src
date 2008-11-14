#	$OpenBSD: Makefile,v 1.4 2008/11/14 11:58:08 millert Exp $

.PATH:		${.CURDIR}/..

LIB=	sudo
SRCS=	alias.c alloc.c defaults.c error.c fileops.c find_path.c gettime.c \
	goodpath.c list.c match.c redblack.c toke.c gram.c zero_bytes.c
CPPFLAGS+=	-I..

CLEANFILES=	gram.c gram.h toke.c

DEBUGLIBS= no
NOPROFILE= yes

.include <bsd.own.mk>
.if !defined(NOPIC)
CPPFLAGS+= -D_PATH_SUDO_NOEXEC=\"/usr/libexec/sudo_noexec.so\"
NOPIC=	yes
.endif

gram.c gram.h: gram.y
	rm -f gram.c gram.h
	${YACC} -d ${.CURDIR}/../gram.y
	mv -f y.tab.c gram.c
	if cmp -s y.tab.h ../gram.h; then rm -f y.tab.h; else mv -f y.tab.h ../gram.h; fi

toke.c: toke.l
	rm -f toke.c
	${LEX} ${.CURDIR}/../toke.l
	mv -f lex.yy.c toke.c

install:
	@echo -n

beforedepend: gram.c toke.c

.include <bsd.lib.mk>
