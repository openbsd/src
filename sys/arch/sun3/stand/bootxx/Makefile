#	$NetBSD: Makefile,v 1.2.2.1 1995/11/18 06:47:36 gwr Exp $

SA_PROG= bootxx
SRCS=   bootxx.c conf.c

all:	${SA_PROG}

# Do not strip or remove a.out header for this one.
install: ${SA_PROG}
	install ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  ${SA_PROG} ${DESTDIR}${MDEC_DIR}/${SA_PROG}

.include <bsd.prog.mk>

