#	$OpenBSD: Makefile.inc,v 1.7 2014/11/30 19:43:56 deraadt Exp $

SRCS+=	bcopy.S memcpy.c memmove.S _memcpy.S \
	index.c rindex.c strchr.c strrchr.c \
	bcmp.c bzero.S ffs.S memchr.c memcmp.S memset.S \
	strcmp.S strncmp.S \
	strcat.c strcpy.c strcspn.c strlen.c strlcat.c strlcpy.c \
	strncat.c  strncpy.c strpbrk.c strrchr.c strsep.c \
	strspn.c swab.c strstr.c

