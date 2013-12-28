#	$OpenBSD: Makefile.inc,v 1.1 2013/12/28 14:45:57 jsing Exp $

CPPFLAGS += -I${.CURDIR}/sparc64
CFLAGS += -DSOFTRAID

.PATH:  ${.CURDIR}/sparc64
SRCS += sparc64_installboot.c
SRCS += sparc64_softraid.c
