#	$OpenBSD: Makefile.inc,v 1.2 2004/02/27 17:36:14 deraadt Exp $

KMINCLUDES= arch/amd64/SYS.h
KMSRCS=	bcmp.S bzero.S ffs.S strcat.S strcmp.S strcpy.S strlen.S \
	htonl.S htons.S ntohl.S ntohs.S
