#	$Id: Makefile,v 1.7 1998/04/26 16:20:06 brian Exp $

PROG=	ppp
SRCS=	arp.c async.c auth.c ccp.c chap.c chap_ms.c chat.c \
	command.c deflate.c defs.c filter.c fsm.c hdlc.c id.c ip.c ipcp.c \
	iplist.c lcp.c log.c lqr.c main.c mbuf.c modem.c os.c \
	pap.c phase.c pred.c route.c server.c sig.c slcompress.c systems.c \
	throughput.c timer.c tun.c vars.c vjcomp.c
CFLAGS+=-Wall -Wmissing-prototypes -DHAVE_DES -DNOALIAS
LDADD+= -lutil -ldes -lz
DPADD+= ${LIBUTIL} ${LIBDES} ${LIBZ}
MAN=	ppp.8
BINMODE=4550
BINOWN=	root
BINGRP=	network

.include <bsd.prog.mk>
