#	$NetBSD: Makefile,v 1.3.2.1 1995/11/18 06:47:45 gwr Exp $

SA_PROG= netboot
SRCS=   boot.c conf.c version.c dev_net.c

DEFS= -DSTANDALONE -DSUN_BOOTPARAMS

all:	${SA_PROG}.bin

.include <bsd.prog.mk>

