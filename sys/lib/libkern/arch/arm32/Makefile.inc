#	$OpenBSD: Makefile.inc,v 1.5 2000/12/18 18:40:45 provos Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	memchr.c memcmp.c \
	bcmp.c bzero.c ffs.c strcat.c strcmp.c strcpy.c strlcat.c strlcpy.c \
	strlen.c strncmp.c \
	strncpy.c scanc.c skpc.c locc.c htonl.c htons.c ntohl.c ntohs.c \
	div.S random.c strncasecmp.c
