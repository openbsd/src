PROG=	ipfstat
MAN=	ipfstat.8
SRCS=	fils.c parse.c opt.c kmem.c 
.PATH:  ${.CURDIR}/../../sbin/ipf
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I${.CURDIR}/../../sbin/ipf



.include <bsd.prog.mk>
