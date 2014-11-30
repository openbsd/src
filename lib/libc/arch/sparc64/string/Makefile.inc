#	$OpenBSD: Makefile.inc,v 1.4 2014/11/30 19:43:56 deraadt Exp $

SRCS+=	bcopy.c memcpy.c memmove.c \
	ffs.S \
	strlen.S \
	bcmp.c bzero.c index.c memchr.c memcmp.c \
	memset.c rindex.c strcat.c strchr.c \
	strcmp.c strcpy.c strcspn.c strlcat.c strlcpy.c \
	strncat.c strncmp.c strncpy.c strpbrk.c strrchr.c strsep.c \
	strspn.c strstr.c swab.c
