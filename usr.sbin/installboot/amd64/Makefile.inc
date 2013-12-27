#	$OpenBSD: Makefile.inc,v 1.1 2013/12/27 14:12:56 jsing Exp $

CPPFLAGS += -I${.CURDIR}/amd64

CFLAGS += -DNLIST
CFLAGS += -DSOFTRAID

SRCS += softraid.c

.PATH:  ${.CURDIR}/i386
SRCS += i386_installboot.c
SRCS += i386_softraid.c
