#	$OpenBSD: Makefile.inc,v 1.1 2013/12/28 13:58:15 jsing Exp $

CPPFLAGS += -I${.CURDIR}/hp300
CFLAGS += -DBOOTSTRAP

.PATH:  ${.CURDIR}/hp300
SRCS += hp300_installboot.c
