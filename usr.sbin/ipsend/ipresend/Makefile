#	$OpenBSD: Makefile,v 1.3 1998/01/26 04:16:56 dgregor Exp $

PROG=	ipresend
BINDIR=	/usr/sbin
MAN=	ipresend.1
SRCS=	ipresend.o resend.o \
	ipft_ef.o ipft_hx.o ipft_pc.o ipft_sn.o ipft_td.o ipft_tx.o opt.o \
	sock.o 44arp.o sbpf.o ip.o 
CFLAGS+=  -DDOSOCKET -I${.CURDIR}/../common -I${.CURDIR}/../../ipftest \
		-I${.CURDIR}/../../../sbin/ipf \
		-I${.CURDIR}/../../../sys/netinet
.PATH:	${.CURDIR}/../common ${.CURDIR}/../../ipftest \
		${.CURDIR}/../../../sbin/ipf

.include <bsd.prog.mk>
