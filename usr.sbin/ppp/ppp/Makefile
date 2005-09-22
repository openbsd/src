#	$OpenBSD: Makefile,v 1.28 2005/09/22 00:08:24 brad Exp $

PROG=	ppp
SRCS=	alias.c alias_cuseeme.c alias_db.c alias_ftp.c alias_irc.c \
	alias_nbt.c alias_pptp.c alias_proxy.c alias_skinny.c alias_smedia.c \
	alias_util.c acf.c arp.c async.c auth.c bundle.c cbcp.c ccp.c chap.c \
	chap_ms.c chat.c command.c datalink.c deflate.c defs.c exec.c filter.c \
	fsm.c hdlc.c iface.c ip.c ipcp.c ipv6cp.c iplist.c lcp.c link.c log.c \
	lqr.c main.c mbuf.c mp.c mppe.c ncp.c ncpaddr.c nat_cmd.c pap.c physical.c \
	pred.c probe.c prompt.c proto.c radius.c radlib.c route.c server.c \
	sig.c slcompress.c systems.c sync.c tcp.c tcpmss.c throughput.c \
	timer.c tty.c tun.c udp.c vjcomp.c
CFLAGS+=-Wall -DNO_FW_PUNCH -DNOI4B -DNONETGRAPH
CFLAGS+=-DLOCALNAT -DLOCALRAD
M4FLAGS=-DLOCALNAT -DLOCALRAD
LDADD+= -lcrypto -ldes -lutil -lz
DPADD+=	${LIBDES} ${LIBUTIL} ${LIBZ} ${LIBCRYPTO}
.if defined(NOSUID) || defined(PPP_NOSUID)
BINMODE=554
.else
BINMODE=4550
BINOWN=	root
.endif
BINGRP=	network
MAN=	ppp.8
CLEANFILES=	ppp.8

.SUFFIXES: .8 .8.m4

.8.m4.8:
	m4 ${M4FLAGS} ${.IMPSRC} >${.TARGET}

NOATM=	/usr/include/netnatm required
.if defined(NOATM)
CFLAGS+=-DNOATM
.else
SRCS+=	atm.c
.endif

.if defined(NOSUID) || defined(PPP_NOSUID)
CFLAGS+=-DNOSUID
.else
SRCS+=	id.c
.endif

CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wno-uninitialized
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wunused
#CDIAGFLAGS+=	-Wsign-compare
CDIAGFLAGS+=	-Wbounded
#CDIAGFLAGS+=	-Wshadow

.include <bsd.prog.mk>
