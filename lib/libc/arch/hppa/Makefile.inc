#	$OpenBSD: Makefile.inc,v 1.2 1999/09/14 00:46:18 mickey Exp $

KMINCLUDES=	arch/hppa/DEFS.h arch/hppa/SYS.h prefix.h
KMSRCS=	bcmp.S bzero.S bcopy.m4 ffs.S

SRCS+=	memmove.S
CLEANFILES+=	memmove.S

memmove.S:	${LIBCSRCDIR}/arch/hppa/string/bcopy.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@m4 -DNAME=bcopy ${.ALLSRC} > ${.TARGET}

