#	$NetBSD: Makefile,v 1.3 1995/09/23 03:42:47 gwr Exp $

SA_PROG= netboot
SRCS=   boot.c conf.c version.c dev_net.c

DEFS= -DSUN_BOOTPARAMS

all:	${SA_PROG}.bin

.include <bsd.prog.mk>

install: ${SA_PROG}.bin
	install ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  ${SA_PROG}.bin ${DESTDIR}${MDEC_DIR}/${SA_PROG}

