#	$NetBSD: Makefile,v 1.2 1995/06/09 22:17:19 gwr Exp $

SA_PROG= bootxx
SRCS=   bootxx.c conf.c

all:	${SA_PROG}

.include <bsd.prog.mk>

# Do not strip or remove a.out header for this one.
install: ${SA_PROG}
	install ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  ${SA_PROG} ${DESTDIR}${MDEC_DIR}/${SA_PROG}

