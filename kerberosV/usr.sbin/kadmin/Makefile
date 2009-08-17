# $OpenBSD: Makefile,v 1.17 2009/08/17 09:40:37 jsg Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kadmin
MAN=	kadmin.8

SRCS=	ank.c \
	cpw.c \
	del.c \
	del_enctype.c \
	dump.c \
	ext.c \
	get.c \
	init.c \
	kadmin.c \
	kadmin-commands.c \
	kadmin-commands.h \
	kadmin_locl.h \
	load.c \
	mod.c \
	pw_quality.c \
	random_password.c \
	rename.c \
	sl.c \
	stash.c \
	util.c
SRCS+=	${PRINTVER}
SRCS+=	tm2time.c

.PATH:	${KRB5SRC}/kadmin
.PATH:	${KRB5SRC}/lib/sl
.PATH:	${ROKENSRC}

slc:	slc-gram.o slc-lex.o ${PRINTVER}
	$(CC) -o slc $> -lkrb5 -lcrypto

SLC=	slc

kadmin:	kadmin-commands.h
kadmin-commands.c kadmin-commands.h:	kadmin-commands.in $(SLC)
	./$(SLC) $(KRB5SRC)/kadmin/kadmin-commands.in
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5 -I.

DPADD+= ${LIBEDIT} ${LIBCURSES} ${LIBKADM5SRV} ${LIBKADM5CLNT} \
	${LIBKRB5} ${LIBCRYPTO} ${LIBDES}
LDADD+=	-ledit -lcurses -lkadm5srv -lkadm5clnt \
	-lkrb5 -lcrypto -ldes

CLEANFILES+=	kadmin-commands.c slc lc-lex.o kadmin-commands.h \
		slc-lex.o slc-gram.o

.include <bsd.prog.mk>
