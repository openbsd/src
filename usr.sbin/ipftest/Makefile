PROG=	ipftest 
MAN=	ipftest.1	
SRCS=	ipt.c fil.c ipft_hx.c ipft_sn.c ipft_ef.c ipft_td.c ipft_pc.c \
	ipft_tx.c misc.c parse.c opt.c ip_frag.c ip_nat.c ip_state.c
.PATH: 	${.CURDIR}/../../sbin/ipf ${.CURDIR}/../../sbin/ipfstat ${.CURDIR}/../../sys/netinet
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I${.CURDIR}/../../sbin/ipf -I${.CURDIR}/../../sys/netinet


.include <bsd.prog.mk>
