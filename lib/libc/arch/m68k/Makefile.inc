#	$OpenBSD: Makefile.inc,v 1.3 1996/08/19 08:13:49 tholo Exp $

KMINCLUDES=	arch/m68k/DEFS.h arch/m68k/SYS.h
KMSRCS=	bcmp.S bzero.S ffs.S strcat.S strcmp.S strcpy.S strlen.S strncmp.S \
	strncpy.S htonl.S htons.S ntohl.S ntohs.S
