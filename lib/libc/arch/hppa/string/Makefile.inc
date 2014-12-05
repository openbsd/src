#	$OpenBSD: Makefile.inc,v 1.9 2014/12/05 01:30:44 deraadt Exp $

SRCS+=	memmove.S memcpy.c \
	index.c rindex.c strchr.c strrchr.c \
	bcmp.c bzero.c ffs.c memchr.c memcmp.c \
	memset.c strcat.c strcmp.c strcpy.c strcspn.c strlen.c \
	strlcat.c strlcpy.c strncat.c strncmp.c strncpy.c \
	strpbrk.c strsep.c strspn.c strstr.c swab.c
