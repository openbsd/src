#	$OpenBSD: Makefile.inc,v 1.8 2015/05/15 22:29:37 millert Exp $

SRCS+=	bcopy.S memcpy.c memmove.S _memcpy.S \
	strchr.c strrchr.c \
	bcmp.c bzero.S ffs.S memchr.c memcmp.S memset.S \
	strcmp.S strncmp.S \
	strcat.c strcpy.c strcspn.c strlen.c strlcat.c strlcpy.c \
	strncat.c  strncpy.c strpbrk.c strrchr.c strsep.c \
	strspn.c swab.c strstr.c

