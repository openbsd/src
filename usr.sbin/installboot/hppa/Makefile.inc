#	$OpenBSD: Makefile.inc,v 1.2 2013/12/28 11:00:33 jsing Exp $

CPPFLAGS += -I${.CURDIR}/hppa
CFLAGS += -DBOOTSTRAP

.PATH:  ${.CURDIR}/hppa
SRCS += hppa_installboot.c
