PROG=	ipnat
MAN=	ipnat.1 ipnat.4 ipnat.5
SRCS=	ipnat.c kmem.c 
.PATH:  ${.CURDIR}/../ipfstat
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I${.CURDIR}/../../sys/netinet

.include <bsd.prog.mk>
