#	$Id: Makefile.inc,v 1.1.1.1 1995/10/18 08:41:23 deraadt Exp $

KMINCLUDES= arch/i386/SYS.h arch/i386/DEFS.h
KMSRCS=	bcmp.S bzero.S ffs.S strcat.S strcmp.S strcpy.S strlen.S \
	htonl.S htons.S ntohl.S ntohs.S
