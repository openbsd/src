PROG=	ipftest 
MAN=	ipftest.1	
SRCS=	ipt.c fil.c ipft_sn.c ipft_ef.c ipft_td.c ipft_pc.c ipft_tx.c misc.c parse.c opt.c
.PATH: 	${.CURDIR}/../../sbin/ipf ${.CURDIR}/../../sbin/ipfstat
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I${.CURDIR}/../../sbin/ipf


.include <bsd.prog.mk>
