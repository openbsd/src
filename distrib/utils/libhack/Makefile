# $OpenBSD: Makefile,v 1.2 2000/03/01 22:10:12 todd Exp $
# $NetBSD: Makefile,v 1.2 1995/10/13 18:10:19 gwr Exp $
#
# Stubs to kill off some things from libc:
# This save space on a boot system.
#

LIB=	hack
SRCS=	getgrent.c gethost.c getnetgr.c getpwent.c

NOPIC=
NOPROFILE=

all: libhack.a

tpwent: getpwent.c
	$(CC) -g -o $@.o -DTEST_MAIN -c getpwent.c
	$(CC) -o $@ $@.o

.include <bsd.lib.mk>
