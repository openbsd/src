#	$Id: Makefile,v 1.4 1998/09/17 00:47:13 brian Exp $

PROG=	ppp
SRCS=	arp.c async.c auth.c bundle.c cbcp.c ccp.c chap.c chap_ms.c chat.c \
	command.c datalink.c deflate.c defs.c filter.c fsm.c hdlc.c id.c \
	ip.c ipcp.c iplist.c lcp.c link.c log.c lqr.c main.c mbuf.c modem.c \
	mp.c pap.c physical.c pred.c probe.c prompt.c route.c server.c \
	sig.c slcompress.c systems.c throughput.c timer.c tun.c vjcomp.c
CFLAGS+=-Wall -DHAVE_DES
LDADD+= -ldes -lutil -lz
DPADD+=	${LIBDES} ${LIBUTIL} ${LIBZ}
BINMODE=4554
BINOWN=	root
BINGRP=	network
MAN=	ppp.8

.if defined(NOALIAS)
CFLAGS+=-DNOALIAS
.else
CFLAGS+=-DNO_FW_PUNCH
SRCS+=	alias.c alias_cmd.c alias_cuseeme.c alias_db.c alias_ftp.c \
	alias_irc.c alias_nbt.c alias_util.c
.endif

.include <bsd.prog.mk>
