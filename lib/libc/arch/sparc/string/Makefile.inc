#	$OpenBSD: Makefile.inc,v 1.3 1996/08/19 08:17:55 tholo Exp $

SRCS+=	bcmp.c bcopy.c bzero.S ffs.S index.c memchr.c memcmp.c memset.c \
	rindex.c strcat.c strcmp.c strcpy.c strcspn.c strlen.S \
	strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.c
LSRCS+=	bzero.c ffs.c strlen.c
