#	$OpenBSD: Makefile.inc,v 1.2 2013/12/27 15:02:49 jsing Exp $

CPPFLAGS += -I${.CURDIR}/amd64

CFLAGS += -DNLIST
CFLAGS += -DSOFTRAID

SRCS += softraid.c

.PATH:  ${.CURDIR}/i386
SRCS += i386_installboot.c
SRCS += i386_nlist.c
SRCS += i386_softraid.c
