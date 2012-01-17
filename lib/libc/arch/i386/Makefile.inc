#	$OpenBSD: Makefile.inc,v 1.3 2012/01/17 21:31:19 mikeb Exp $

KMINCLUDES= arch/i386/SYS.h arch/i386/DEFS.h
KMSRCS=	bcmp.S bzero.S ffs.S strcat.S strcmp.S strcpy.S htonl.S htons.S \
	ntohl.S ntohs.S
