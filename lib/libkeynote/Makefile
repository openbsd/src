#	$OpenBSD: Makefile,v 1.11 2000/01/25 09:08:10 angelos Exp $

LIB=	keynote
MAN=	keynote.3 keynote.4 keynote.5
MLINKS=	keynote.3 kn_init.3 keynote.3 kn_add_assertion.3 \
        keynote.3 kn_remove_assertion.3 keynote.3 kn_add_action.3 \
        keynote.3 kn_remove_action.3 keynote.3 kn_add_authorizer.3 \
        keynote.3 kn_remove_authorizer.3 keynote.3 kn_do_query.3 \
        keynote.3 kn_get_failed.3 keynote.3 kn_cleanup_action_environment.3 \
        keynote.3 kn_close.3 keynote.3 kn_query.3 keynote.3 \
        kn_read_asserts.3 keynote.3 kn_keycompare.3 keynote.3 \
        kn_get_authorizer.3 keynote.3 kn_get_licensees.3 keynote.3 \
        kn_encode_base64.3 keynote.3 kn_decode_base64.3 keynote.3 \
        kn_encode_hex.3 keynote.3 kn_decode_hex.3 keynote.3 \
        kn_encode_key.3 keynote.3 kn_decode_key.3 keynote.3 \
        kn_sign_assertion.3 keynote.3 kn_verify_assertion.3
NOPIC=	
CFLAGS+= -Wall -DCRYPTO -DHAVE_CONFIG_H -I. -I${.CURDIR}
LEXFLAGS = -Cr -Pkn -s -i
YACCFLAGS = -d -p kn -b k

HDRS=	keynote.h
SRCS=	k.tab.c lex.kn.c environment.c parse_assertion.c signature.c auxil.c \
	base64.c

CLEANFILES+= k.tab.c lex.kn.c k.tab.h

k.tab.c: keynote.y keynote.h signature.h
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
