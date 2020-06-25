#	$OpenBSD: Makefile.inc,v 1.1 2020/06/25 02:34:22 drahn Exp $

SRCS+=	memcpy.c memmove.S \
	strchr.c strrchr.c \
	bcmp.c bzero.c ffs.c memchr.c memcmp.c memset.c strcat.c \
	strcmp.c strcpy.c strcspn.c strlen.c strlcat.c strlcpy.c \
	strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.c
