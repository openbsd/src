#	$NetBSD: Makefile,v 1.2 1995/10/17 22:58:10 gwr Exp $

SA_PROG= tapeboot
SRCS=   boot.c conf.c rawfs.c dev_tape.c version.c

# DEFS= -DDEBUG

all:	${SA_PROG}.bin

.include <bsd.prog.mk>

install: ${SA_PROG}.bin
	install ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  ${SA_PROG}.bin ${DESTDIR}${MDEC_DIR}/${SA_PROG}

