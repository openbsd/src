#	$NetBSD: Makefile,v 1.2 1995/06/09 22:24:06 gwr Exp $

SA_PROG= ufsboot
SRCS=   boot.c conf.c version.c

all:	${SA_PROG}.bin

.include <bsd.prog.mk>

install: ${SA_PROG}.bin
	install ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  ${SA_PROG}.bin ${DESTDIR}${MDEC_DIR}/${SA_PROG}

