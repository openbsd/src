PROG=	ipsend
NOMAN=		
SRCS=	iptests.c ip.c ipsend.c ipsopt.c resend.c sbpf.c sock.c 44arp.c ipft_sn.c ipft_pc.c
.PATH:	${.CURDIR}/../../usr.sbin/ipftest
CFLAGS+=  -DDOSOCKET -I${.CURDIR}/../../usr.sbin/ipftest -I${.CURDIR}/../../sbin/ipf -I${.CURDIR}/../../sys/netinet
LDADD+= -lpcap


.include <bsd.prog.mk>
