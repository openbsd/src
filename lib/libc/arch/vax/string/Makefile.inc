#	$OpenBSD: Makefile.inc,v 1.9 2014/11/30 19:43:56 deraadt Exp $

SRCS+=	bcopy.S memcpy.c memmove.S \
	bcmp.S bzero.S ffs.S memchr.c memcmp.S \
	memset.S \
	strchr.S rindex.c strrchr.c \
	strcat.c strcmp.c strcpy.c strcspn.c strlen.c strlcat.c \
	strlcpy.c strncat.c strncmp.c strncpy.c strpbrk.c \
	strsep.c strspn.c strstr.c swab.c
