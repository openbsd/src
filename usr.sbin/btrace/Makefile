# $OpenBSD: Makefile,v 1.1 2020/01/21 16:24:55 mpi Exp $

PROG=		btrace
MAN=		bt.5 btrace.8

SRCS=		bt_parse.y btrace.c ksyms.c map.c printf.c

# Use syscall names generated from sys/kern/makesyscalls.sh
SRCS+=		syscalls.c
CFLAGS+=	-DPTRACE -DKTRACE -DACCOUNTING -DNFSCLIENT -DSYSVSHM -DSYSVSEM
CFLAGS+=	-DSYSVMSG
.PATH:		${.CURDIR}/../../sys/kern

CFLAGS+=	-W -Wall -Wstrict-prototypes -Wno-unused -Wunused-variable
CFLAGS+=	-I${.CURDIR}

LDADD+=		-lelf
DPADD+=		${LIBELF}

DEBUG?=		-g

.include <bsd.prog.mk>
