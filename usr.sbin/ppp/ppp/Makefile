#	$OpenBSD: Makefile,v 1.14 2000/02/27 01:38:24 brian Exp $

PROG=	ppp
SRCS=	alias.c alias_cuseeme.c alias_db.c alias_ftp.c alias_irc.c \
	alias_nbt.c alias_proxy.c alias_util.c acf.c arp.c async.c auth.c \
	bundle.c cbcp.c ccp.c chap.c chap_ms.c chat.c command.c datalink.c \
	deflate.c defs.c exec.c filter.c fsm.c hdlc.c id.c iface.c ip.c \
	ipcp.c iplist.c lcp.c link.c log.c lqr.c main.c mbuf.c mp.c nat_cmd.c \
	pap.c physical.c pred.c probe.c prompt.c proto.c radius.c radlib.c \
	route.c server.c sig.c slcompress.c systems.c sync.c tcp.c \
	throughput.c timer.c tty.c tun.c udp.c vjcomp.c
CFLAGS+=-Wall -DHAVE_DES -DNO_FW_PUNCH -DNOI4B -DNONETGRAPH -DLOCALNAT -DLOCALRAD
LDADD+= -ldes -lutil -lz
DPADD+=	${LIBDES} ${LIBUTIL} ${LIBZ}
BINMODE=4554
BINOWN=	root
BINGRP=	network
MAN=	ppp.8

.include <bsd.prog.mk>
