#	$OpenBSD: Makefile.inc,v 1.1 2013/12/27 14:23:49 jsing Exp $

CPPFLAGS += -I${.CURDIR}/hppa

SRCS += bootstrap.c

.PATH:  ${.CURDIR}/hppa
SRCS += hppa_installboot.c
