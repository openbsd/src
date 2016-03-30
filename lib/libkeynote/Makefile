#	$OpenBSD: Makefile,v 1.20 2016/03/30 06:38:42 jmc Exp $

LIB=	keynote
MAN=	keynote.3 keynote.4 keynote.5
NOPIC=	
CFLAGS+= -Wall -I. -I${.CURDIR}
LEXFLAGS = -Cr -Pkn -s -i
YACCFLAGS = -d -p kn -b k

HDRS=	keynote.h
SRCS=	k.tab.c lex.kn.c environment.c parse_assertion.c signature.c auxil.c \
	base64.c

CLEANFILES+= k.tab.c lex.kn.c k.tab.h

k.tab.c k.tab.h: keynote.y keynote.h signature.h
	$(YACC.y) $(YACCFLAGS) ${.CURDIR}/keynote.y

lex.kn.c: keynote.l keynote.y keynote.h assertion.h signature.h
	$(LEX.l) $(LEXFLAGS) ${.CURDIR}/keynote.l

includes:
	@@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		$$i ${DESTDIR}/usr/include"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.lib.mk>
