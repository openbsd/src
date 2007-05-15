#	$OpenBSD: Makefile.inc,v 1.2 2007/05/15 18:42:31 otto Exp $

SRCS+=  bcmp.c ffs.S index.c memchr.c memcmp.c bcopy.c bzero.c \
        rindex.c strcat.c strcmp.c strcpy.c strcspn.c strlen.S \
        strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
        strspn.c strstr.c swab.c memset.c strlcpy.c strlcat.c

LSRCS+=	ffs.c strlen.c
