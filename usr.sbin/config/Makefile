#	$OpenBSD: Makefile,v 1.9 2001/09/07 20:47:18 brad Exp $

PROG=	config
BINDIR=	/usr/sbin
SRCS=	files.c gram.y hash.c main.c mkheaders.c mkioconf.c mkmakefile.c \
	mkswap.c pack.c scan.l sem.c util.c \
	ukc.c misc.c ukcutil.c cmd.c exec.c
.if (${MACHINE_ARCH} == "powerpc") || (${MACHINE_ARCH} == "hppa") || \
    (${MACHINE_ARCH} == "alpha") || (${MACHINE_ARCH} == "sparc64")
CFLAGS+=-DELF_SUPPORT
SRCS+=  exec_elf.c
.else
CFLAGS+=-DAOUT_SUPPORT
SRCS+=  exec_aout.c
.endif

CFLAGS+=-I${.CURDIR} -I.

# This program actually requires "flex" (not just any old lex).
# Also note that it does not use -ll
LEX=flex

LDADD=	-lkvm
DPADD=	${LIBKVM}

CLEANFILES=gram.c scan.c y.tab.h
MAN=	config.8

.include <bsd.prog.mk>

.depend: gram.c scan.c
