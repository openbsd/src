#	$OpenBSD: Makefile.inc,v 1.7 2014/11/30 19:43:56 deraadt Exp $

SRCS+=	bcopy.S memcpy.c memmove.S \
	strchr.S strrchr.S \
	bcmp.S bcopy.S bzero.S ffs.S memchr.S memcmp.S swab.S \
	memset.S strcat.S strcmp.S strcpy.S \
	strcspn.c strlen.c strlcat.c strlcpy.c \
	strncat.c strncmp.S strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c
