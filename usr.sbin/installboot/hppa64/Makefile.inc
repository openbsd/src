#	$OpenBSD: Makefile.inc,v 1.1 2013/12/28 13:58:15 jsing Exp $

CPPFLAGS += -I${.CURDIR}/hppa64
CFLAGS += -DBOOTSTRAP

.PATH:  ${.CURDIR}/hppa64
SRCS += hppa64_installboot.c
