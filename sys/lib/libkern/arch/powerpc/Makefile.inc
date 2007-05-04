#	$OpenBSD: Makefile.inc,v 1.13 2007/05/04 22:24:34 deraadt Exp $
#	$NetBSD: Makefile.inc,v 1.6 1994/10/26 06:39:03 cgd Exp $

CFLAGS	+= -D_KERNEL

SRCS+=	__main.c __eabi.c \
	imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	memchr.c memcmp.c memset.c \
	bcmp.c ffs.c strcmp.c strlcat.c strlcpy.c \
	strlen.c strncmp.c \
	strncpy.c scanc.c skpc.c \
	memcpy.c random.c strncasecmp.c

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
				 
