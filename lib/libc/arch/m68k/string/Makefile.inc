#	$OpenBSD: Makefile.inc,v 1.4 1996/08/19 08:15:05 tholo Exp $

SRCS+=	bcmp.S bcopy.S bzero.S ffs.S index.S memchr.c memcmp.S memset.S \
	rindex.S strcat.S strcmp.S strcpy.S strcspn.c strlen.S \
	strncat.c strncmp.S strncpy.S strpbrk.c strsep.c \
	strspn.c strstr.c swab.S
SRCS+=	memcpy.S memmove.S strchr.S strrchr.S
LSRCS+=	bcmp.c bcopy.c bzero.c ffs.c index.c memcmp.c memset.c rindex.c \
	strcmp.c strcpy.c strlen.c strncmp.c strncpy.c swab.c
