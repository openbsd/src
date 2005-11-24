# $OpenBSD: Makefile,v 1.9 2005/11/24 20:49:20 deraadt Exp $

LIB=	sectok
WANTLINT=
SRCS=	atr.c cmdtab.c cyberflex.c input.c r1r2.c sc7816.c sectok.c todos_atr.c \
	ifdhandler.c scT1.c scio.c scrw.c
HDRS=	sectok.h
MAN=	sectok.3

MLINKS+=sectok.3 sectok_open.3
MLINKS+=sectok.3 sectok_friendly_open.3
MLINKS+=sectok.3 sectok_xopen.3
MLINKS+=sectok.3 sectok_reset.3
MLINKS+=sectok.3 sectok_apdu.3
MLINKS+=sectok.3 sectok_cardpresent.3
MLINKS+=sectok.3 sectok_close.3
MLINKS+=sectok.3 sectok_selectfile.3
MLINKS+=sectok.3 sectok_fmt_fid.3
MLINKS+=sectok.3 sectok_parse_atr.3
MLINKS+=sectok.3 sectok_parse_fname.3
MLINKS+=sectok.3 sectok_parse_input.3
MLINKS+=sectok.3 sectok_get_input.3
MLINKS+=sectok.3 sectok_fdump_reply.3
MLINKS+=sectok.3 sectok_dump_reply.3
MLINKS+=sectok.3 sectok_print_sw.3
MLINKS+=sectok.3 sectok_get_sw.3
MLINKS+=sectok.3 sectok_get_ins.3
MLINKS+=sectok.3 sectok_swOK.3

CFLAGS+=-Wall

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/`basename $$i` || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.lib.mk>
