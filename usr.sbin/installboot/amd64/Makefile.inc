#	$OpenBSD: Makefile.inc,v 1.3 2013/12/28 11:00:33 jsing Exp $

CPPFLAGS += -I${.CURDIR}/amd64
CFLAGS += -DSOFTRAID

.PATH:  ${.CURDIR}/i386
SRCS += i386_installboot.c
SRCS += i386_nlist.c
SRCS += i386_softraid.c
