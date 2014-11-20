#	$OpenBSD: Makefile.inc,v 1.5 2014/11/20 14:33:00 reyk Exp $

SRCS+=  bcmp.S ffs.S index.c memchr.S memcmp.S bcopy.S bzero.S \
        rindex.c strcat.c strcmp.S strcpy.c strcspn.c strlen.c \
        strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
        strspn.c strstr.c swab.c memset.S strlcpy.c strlcat.c
SRCS+=	memcpy.S memmove.S strchr.S strrchr.S
