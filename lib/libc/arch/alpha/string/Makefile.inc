#	$OpenBSD: Makefile.inc,v 1.8 2014/11/30 19:43:54 deraadt Exp $

SRCS+=	bcopy.S memcpy.c memmove.S \
	index.c rindex.c strchr.c strrchr.c \
	bzero.S ffs.S \
	memchr.c memcmp.c memset.c strcat.c bcmp.c \
	strcmp.c strcpy.c strcspn.c strlen.c strlcat.c strlcpy.c \
	strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.c
