#	$OpenBSD: Makefile.inc,v 1.6 2002/09/10 03:42:25 mickey Exp $

KMINCLUDES=	arch/hppa/DEFS.h arch/hppa/SYS.h prefix.h
KMSRCS=

CLEANFILES+=	cerror.S

cerror.S:
	@echo > cerror.S
