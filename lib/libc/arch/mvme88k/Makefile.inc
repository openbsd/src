#	$OpenBSD: Makefile.inc,v 1.2 2000/03/01 17:31:22 todd Exp $

KMINCLUDES=	arch/m88k/DEFS.h arch/m88k/SYS.h
#KMSRCS=	bcmp.S bzero.S ffs.S strcat.c strcmp.S strcpy.S strlen.S strncmp.S \
#	strncpy.S htonl.S htons.S ntohl.S ntohs.S
KMSRCS=	htonl.S htons.S ntohl.S ntohs.S
