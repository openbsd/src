#	$OpenBSD: Makefile.inc,v 1.2 2004/02/01 05:47:10 drahn Exp $
#	$NetBSD: Makefile.inc,v 1.3 2001/11/20 00:29:19 chris Exp $

SRCS+=	__main.c byte_swap_2.S byte_swap_4.S \
	bcmp.c ffs.c imax.c imin.c lmax.c lmin.c max.c min.c random.c scanc.c \
	skpc.c strcmp.c strlen.c strncmp.c strncasecmp.c \
	strlcat.c strlcpy.c \
	strncpy.c ulmax.c ulmin.c 


SRCS+=	bcopy.S divsi3.S
SRCS+=	memchr.c memcmp.c memcpy.S memset.S

OBJS+=	bzero.o
CLEANFILES+=	bzero.o

bzero.o: ${KERNDIR}/memset.c
	@echo "${COMPILE.c} -DBZERO ${KERNDIR}/memset.c -o ${.TARGET}"
	@${COMPILE.c} -DBZERO ${KERNDIR}/memset.c  -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o
				 
bzero.po: ${KERNDIR}/memset.c
	@echo "${COMPILE.c} -DBZERO ${KERNDIR}/memset.c -o ${.TARGET}"
	@${COMPILE.c} -DBZERO ${KERNDIR}/memset.c  -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o
				 
