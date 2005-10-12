# $OpenBSD: Makefile,v 1.9 2005/10/12 16:08:14 jmc Exp $

LIB=	skey
SRCS=	skeylogin.c skeysubr.c put.c
HDRS=	skey.h
MAN+=	skey.3 skey.5
MLINKS+=skey.3 atob8.3 skey.3 backspace.3 skey.3 btoa8.3 skey.3 btoe.3 \
	skey.3 etob.3 skey.3 f.3 skey.3 htoi.3 skey.3 keycrunch.3 \
	skey.3 put8.3 skey.3 readpass.3 skey.3 readskey.3 skey.3 rip.3 \
	skey.3 sevenbit.3 skey.3 skey_authenticate.3 \
	skey.3 skey_get_algorithm.3 skey.3 skey_haskey.3 skey.3 skey_keyinfo.3 \
	skey.3 skey_passcheck.3 skey.3 skey_set_algorithm.3 \
	skey.3 skey_unlock.3 skey.3 skeychallenge.3 skey.3 skeychallenge2.3 \
	skey.3 skeygetnext.3 skey.3 skeylookup.3 skey.3 skeyverify.3 \
	skey.3 skipspace.3

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/`basename $$i` || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.lib.mk>
