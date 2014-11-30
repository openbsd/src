#	$OpenBSD: Makefile.inc,v 1.7 2014/11/30 19:43:56 deraadt Exp $

SRCS+=	memcpy.c memmove.S \
	strchr.S strrchr.S \
	bcmp.S bzero.S ffs.S memchr.S \
	memset.S strcat.c strcmp.S \
	memcmp.c strcpy.c strcspn.c strlen.c strlcat.c \
	strlcpy.c strncat.c strncmp.c strncpy.c strpbrk.c \
	strsep.c strspn.c strstr.c swab.c
