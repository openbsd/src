#	$OpenBSD: Makefile.inc,v 1.15 2002/09/15 09:49:01 mickey Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	random.c ashrdi3.c divdi3.c strncpy.c strncmp.c strcmp.c memchr.c \
	memcmp.c memset.c strlen.c strcpy.c strlcpy.c strlcat.c \
	strcat.c skpc.c strncasecmp.c \
	bcmp.c scanc.c ffs.c

SRCS+=	milli.S
OBJS+=	bzero.o

SRCS+=	bcopy.S spcopy.S
CLEANFILES+=	bcopy.S spcopy.S bzero.o

bzero.o: ${KERNDIR}/memset.c
	@echo "${COMPILE.c} -DBZERO ${KERNDIR}/memset.c -o ${.TARGET}"
	@${COMPILE.c} -DBZERO ${KERNDIR}/memset.c  -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

bcopy.S: ${M}/bcopy.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@m4 -DNAME=bcopy ${.ALLSRC} > ${.TARGET}

spcopy.S: ${M}/bcopy.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@m4 -DNAME=spcopy ${.ALLSRC} > ${.TARGET}
