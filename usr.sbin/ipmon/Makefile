PROG=	ipmon
MAN=	ipmon.8	
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -DLOGFAC=LOG_LOCAL0 \
	-I${.CURDIR}/../../sys/netinet


.include <bsd.prog.mk>
