#	$OpenBSD: Makefile,v 1.10 2002/07/24 18:39:01 art Exp $

.include <bsd.own.mk>

PROG=	config
BINDIR=	/usr/sbin
SRCS=	files.c gram.y hash.c main.c mkheaders.c mkioconf.c mkmakefile.c \
	mkswap.c pack.c scan.l sem.c util.c \
	ukc.c misc.c ukcutil.c cmd.c exec.c
.if (${ELF_TOOLCHAIN} == "yes")
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
