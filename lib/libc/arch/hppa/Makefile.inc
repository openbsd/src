#	$OpenBSD: Makefile.inc,v 1.4 2002/08/08 05:57:26 mickey Exp $

KMINCLUDES=	arch/hppa/DEFS.h arch/hppa/SYS.h prefix.h
KMSRCS=	bcopy.m4 ffs.S

SRCS+=	memmove.S
CLEANFILES+=	memmove.S cerror.S

cerror.S:
	@echo > cerror.S

memmove.S:	${LIBCSRCDIR}/arch/hppa/string/bcopy.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@m4 -DNAME=bcopy ${.ALLSRC} > ${.TARGET}

