#	$OpenBSD: Makefile.inc,v 1.4 2014/11/30 19:43:56 deraadt Exp $

SRCS+=	bcopy.S memcpy.c memmove.S \
	bzero.S ffs.S memset.S \
	index.c rindex.c strchr.c strrchr.c \
	bcmp.c memcmp.c memchr.c strcat.c strcmp.c strcpy.c strcspn.c \
	strlen.c strlcat.c strlcpy.c strncat.c strncmp.c strncpy.c \
	strpbrk.c strsep.c strspn.c strstr.c swab.c
