#	$OpenBSD: Makefile.inc,v 1.1 2004/01/28 01:44:44 mickey Exp $

KMINCLUDES= arch/x86_64/SYS.h
KMSRCS=	bcmp.S bzero.S ffs.S strcat.S strcmp.S strcpy.S strlen.S \
	htonl.S htons.S ntohl.S ntohs.S
