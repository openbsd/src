#	$OpenBSD: Makefile.inc,v 1.2 1996/08/19 08:12:11 tholo Exp $

KMINCLUDES= arch/i386/SYS.h arch/i386/DEFS.h
KMSRCS=	bcmp.S bzero.S ffs.S strcat.S strcmp.S strcpy.S strlen.S \
	htonl.S htons.S ntohl.S ntohs.S
