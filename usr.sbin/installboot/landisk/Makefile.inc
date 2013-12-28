#	$OpenBSD: Makefile.inc,v 1.1 2013/12/28 13:58:15 jsing Exp $

CPPFLAGS += -I${.CURDIR}/landisk
CFLAGS += -DBOOTSTRAP

.PATH:  ${.CURDIR}/landisk
SRCS += landisk_installboot.c
