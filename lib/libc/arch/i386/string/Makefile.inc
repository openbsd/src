#	$NetBSD: Makefile.inc,v 1.1 1995/03/20 14:45:42 mycroft Exp $

SRCS+=	bcmp.S bcopy.S bzero.S ffs.S index.S memchr.S memcmp.S memset.S \
	rindex.S strcat.S strcmp.S strcpy.S strcspn.c strlen.S \
	strncat.c strncmp.S strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.S
SRCS+=	memcpy.S memmove.S strchr.S strrchr.S
LSRCS+=	bcmp.c bcopy.c bzero.c ffs.c index.c memchr.c memcmp.c memset.c \
	rindex.c strcat.c strcmp.c strcpy.c strlen.c strncmp.c swab.c
