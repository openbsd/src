#	$OpenBSD: Makefile.inc,v 1.1 2006/10/06 21:19:15 mickey Exp $

SRCS+=	__main.c \
	bcmp.c imax.c imin.c lmax.c lmin.c max.c min.c random.c scanc.c \
	skpc.c strcmp.c strlen.c strncmp.c strncasecmp.c \
	strlcat.c strlcpy.c \
	strncpy.c ulmax.c ulmin.c 

SRCS+=	ffs.S movstr_i4.S sdivsi3.S udivsi3.S
SRCS+=	memchr.c memcmp.c memcpy.S memset.S

OBJS+=	bzero.o bcopy.o memmove.o
CLEANFILES+=	bzero.o bcopy.o memmove.o

bzero.o: ${M}/memset.S
	@echo "${COMPILE.c} -DBZERO ${M}/memset.S -o ${.TARGET}"
	@${COMPILE.c} -DBZERO ${M}/memset.S  -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

bzero.po: ${M}/memset.S
	@echo "${COMPILE.c} -DBZERO ${M}/memset.S -o ${.TARGET}"
	@${COMPILE.c} -DBZERO ${M}/memset.S -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

bcopy.o: ${M}/memcpy.S
	@echo "${COMPILE.c} -DBCOPY ${M}/memcpy.S -o ${.TARGET}"
	@${COMPILE.c} -DBCOPY ${M}/memcpy.S  -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

bcopy.po: ${M}/memcpy.S
	@echo "${COMPILE.c} -DBCOPY ${M}/memcpy.S -o ${.TARGET}"
	@${COMPILE.c} -DBCOPY ${M}/memcpy.S  -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

memmove.o: ${M}/memcpy.S
	@echo "${COMPILE.c} -DMEMMOVE ${M}/memcpy.S -o ${.TARGET}"
	@${COMPILE.c} -DMEMMOVE ${M}/memcpy.S  -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

memmove.po: ${M}/memcpy.S
	@echo "${COMPILE.c} -DMEMMOVE ${M}/memcpy.S -o ${.TARGET}"
	@${COMPILE.c} -DMEMMOVE ${M}/memcpy.S  -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

