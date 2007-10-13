#	$OpenBSD: Makefile.inc,v 1.2 2007/10/13 19:28:14 kettenis Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	random.c strncpy.c strncmp.c strcmp.c memchr.c \
	memcmp.c memset.c strlen.c strlcpy.c strlcat.c \
	skpc.c strncasecmp.c \
	bcmp.c scanc.c ffs.c
SRCS+=  divdi3.c moddi3.c muldi3.c qdivrem.c udivdi3.c umoddi3.c 

SRCS+=	milli.S
OBJS+=	bzero.o

SRCS+=	bcopy.S spcopy.S
CLEANFILES+=	bcopy.S spcopy.S bzero.o

bzero.o: ${KERNDIR}/memset.c
	@echo "${COMPILE.c} -DBZERO ${KERNDIR}/memset.c -o ${.TARGET}"
	@${COMPILE.c} -DBZERO ${KERNDIR}/memset.c  -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

bzero.po: ${KERNDIR}/memset.c
	@echo "${COMPILE.c} -p -DBZERO ${KERNDIR}/memset.c -o ${.TARGET}"
	@${COMPILE.c} -p -DBZERO ${KERNDIR}/memset.c  -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

bcopy.S: ${M}/bcopy.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@m4 -DNAME=bcopy ${.ALLSRC} > ${.TARGET}

spcopy.S: ${M}/bcopy.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@m4 -DNAME=spcopy ${.ALLSRC} > ${.TARGET}
