#	$Id: Makefile,v 1.2 1997/11/26 01:37:32 brian Exp $

PROG=	ppp
SRCS=	arp.c async.c auth.c ccp.c chap.c chap_ms.c chat.c \
	command.c defs.c filter.c fsm.c hdlc.c id.c ip.c ipcp.c lcp.c \
	log.c lqr.c main.c mbuf.c modem.c os.c pap.c phase.c \
	pred.c route.c server.c sig.c slcompress.c systems.c throughput.c \
	timer.c tun.c vars.c vjcomp.c
CFLAGS+=-Wall -Wmissing-prototypes -DHAVE_DES
LDADD+= -lutil -ldes
DPADD+= ${LIBUTIL} ${LIBDES}
MAN=	ppp.8
BINMODE=4550
BINOWN=	root
BINGRP=	network

.if defined(HAVE_LIBALIAS)
SRCS+=	alias_cmd.c loadalias.c
.else
CFLAGS+=-DNOALIAS
.endif

.include <bsd.prog.mk>
