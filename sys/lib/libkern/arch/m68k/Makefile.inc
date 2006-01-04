#	$OpenBSD: Makefile.inc,v 1.9 2006/01/04 22:32:46 miod Exp $
#	$NetBSD: Makefile.inc,v 1.7 1996/04/18 01:53:04 cgd Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	memchr.c memcmp.S memset.S random.c \
	bcmp.S bzero.S ffs.S strcmp.S strlcat.c strlcpy.c \
	strlen.S strncmp.S \
	strncpy.S htonl.S htons.S ntohl.S ntohs.S scanc.S skpc.S locc.S \
	strncasecmp.c
