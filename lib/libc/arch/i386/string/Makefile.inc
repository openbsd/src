#	$OpenBSD: Makefile.inc,v 1.8 2014/12/02 04:04:30 deraadt Exp $

SRCS+=	memcpy.c memmove.S \
	strchr.S strrchr.S \
	bcmp.S bzero.S ffs.S memchr.S memcmp.S swab.S \
	memset.S strcat.S strcmp.S strcpy.S \
	strcspn.c strlen.c strlcat.c strlcpy.c \
	strncat.c strncmp.S strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c
