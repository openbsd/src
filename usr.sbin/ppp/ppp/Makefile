#	$Id: Makefile,v 1.6 1999/02/06 03:22:30 brian Exp $

PROG=	ppp
SRCS=	alias.c alias_cmd.c alias_cuseeme.c alias_db.c alias_ftp.c \
	alias_irc.c alias_nbt.c alias_util.c arp.c async.c auth.c bundle.c \
	cbcp.c ccp.c chap.c chap_ms.c chat.c command.c datalink.c deflate.c \
	defs.c filter.c fsm.c hdlc.c id.c iface.c ip.c ipcp.c iplist.c lcp.c \
	link.c log.c lqr.c main.c mbuf.c modem.c mp.c pap.c physical.c pred.c \
	probe.c prompt.c radius.c radlib.c route.c server.c sig.c slcompress.c \
	systems.c throughput.c timer.c tun.c vjcomp.c
CFLAGS+=-Wall -DHAVE_DES -DNO_FW_PUNCH
LDADD+= -ldes -lutil -lz
DPADD+=	${LIBDES} ${LIBUTIL} ${LIBZ}
BINMODE=4554
BINOWN=	root
BINGRP=	network
MAN=	ppp.8


.include <bsd.prog.mk>
