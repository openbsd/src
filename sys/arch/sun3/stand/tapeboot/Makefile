#	$NetBSD: Makefile,v 1.1.1.1 1995/10/13 21:27:30 gwr Exp $

SA_PROG= tapeboot
SRCS=   boot.c conf.c dev_tape.c version.c

all:	${SA_PROG}.bin

.include <bsd.prog.mk>

install: ${SA_PROG}.bin
	install ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  ${SA_PROG}.bin ${DESTDIR}${MDEC_DIR}/${SA_PROG}

