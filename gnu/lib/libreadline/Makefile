#	$OpenBSD: Makefile,v 1.2 2001/03/18 20:04:11 millert Exp $

LIB=	readline
SRCS=	readline.c funmap.c keymaps.c vi_mode.c parens.c rltty.c complete.c \
	bind.c isearch.c display.c signals.c util.c kill.c undo.c macro.c \
	input.c callback.c terminal.c xmalloc.c history.c histsearch.c \
	histexpand.c histfile.c nls.c search.c shell.c savestring.c tilde.c
HDRS=	readline.h chardefs.h keymaps.h history.h tilde.h rlstdc.h rlconf.h
CPPFLAGS+=-DHAVE_CONFIG_H -I${.CURDIR}
SUBDIRS= doc

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/readline/$$i || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include/readline"; \
	    echo $$j; \
	    eval "$$j"; \
	done

cleandir:
	@cd ${.CURDIR}/doc && ${MAKE} cleandir

.include <bsd.lib.mk>
