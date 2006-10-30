#	$OpenBSD: Makefile,v 1.14 2006/10/30 20:25:48 otto Exp $

.include <bsd.own.mk>

PROG=	config
SRCS=	files.c gram.y hash.c main.c mkheaders.c mkioconf.c mkmakefile.c \
	mkswap.c pack.c scan.l sem.c util.c \
	ukc.c misc.c ukcutil.c cmd.c exec.c
.if (${ELF_TOOLCHAIN:L} == "yes")
CFLAGS+=-DELF_SUPPORT
SRCS+=  exec_elf.c
.else
CFLAGS+=-DAOUT_SUPPORT
SRCS+=  exec_aout.c
.endif

CFLAGS+=-I${.CURDIR} -I.

LDADD=	-lkvm
DPADD=	${LIBKVM}

MAN=	config.8

.include <bsd.prog.mk>
