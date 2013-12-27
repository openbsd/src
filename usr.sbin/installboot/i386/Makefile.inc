#	$OpenBSD: Makefile.inc,v 1.1 2013/12/27 13:52:40 jsing Exp $

CPPFLAGS += -I${.CURDIR}/i386
CFLAGS += -DSOFTRAID

SRCS += softraid.c

.PATH:  ${.CURDIR}/i386
SRCS += i386_installboot.c
SRCS += i386_softraid.c
