#	$OpenBSD: Makefile.inc,v 1.7 1999/05/19 17:10:51 mickey Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	random.c ashrdi3.c divdi3.c strncpy.c strncmp.c strcmp.c memchr.c \
	memset.c strlen.c strcpy.c strcat.c skpc.c strncasecmp.c scanc.c

SRCS+=	milli.S bzero.S bcmp.S ffs.S

SRCS+=	bcopy.S spcopy.S
CLEANFILES+=	bcopy.S spcopy.S

bcopy.S: ${M}/bcopy.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@m4 -DNAME=bcopy ${.ALLSRC} > ${.TARGET}

spcopy.S: ${M}/bcopy.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@m4 -DNAME=spcopy ${.ALLSRC} > ${.TARGET}
