#	$OpenBSD: Makefile,v 1.6 1998/09/15 10:05:46 pattonme Exp $

PROG=	ipftest 
MAN=	ipftest.1	
SRCS=	ipt.c fil.c ipft_hx.c ipft_sn.c ipft_ef.c ipft_td.c ipft_pc.c \
	ipft_tx.c misc.c parse.c opt.c ip_frag.c ip_nat.c ip_state.c \
	ip_auth.c ip_fil.c ip_proxy.c

.PATH: 	${.CURDIR}/../../sbin/ipf ${.CURDIR}/../../sbin/ipfstat \
	${.CURDIR}/../../sys/netinet

CFLAGS+=-I${.CURDIR}/../../sbin/ipf \
	-I${.CURDIR}


.include <bsd.prog.mk>
