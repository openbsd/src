#	$NetBSD: Makefile,v 1.4 1995/11/17 23:23:55 gwr Exp $

SA_PROG= netboot
SRCS=   boot.c conf.c version.c dev_net.c

DEFS= -DSTANDALONE -DSUN_BOOTPARAMS

all:	${SA_PROG}.bin

.include <bsd.prog.mk>

