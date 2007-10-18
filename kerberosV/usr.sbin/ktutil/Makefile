# $OpenBSD: Makefile,v 1.13 2007/10/18 18:20:20 deraadt Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	ktutil
MAN=	ktutil.8

SRCS=	add.c \
	change.c \
	copy.c \
	get.c \
	ktutil.c \
	ktutil-commands.c \
	list.c \
	purge.c \
	remove.c \
	rename.c \
	sl.c
SRCS+=	${PRINTVER}

.PATH:	${KRB5SRC}/admin
.PATH:	${KRB5SRC}/lib/sl

slc:	slc-gram.o slc-lex.o ${PRINTVER}
	$(CC) -o slc $> -lkrb5 -lcrypto

SLC=	slc

ktutil:	ktutil-commands.h
ktutil-commands.c ktutil-commands.h:	ktutil-commands.in $(SLC)
	./$(SLC) $(KRB5SRC)/admin/ktutil-commands.in

CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5 -I.

DPADD+=	${LIBREADLINE} ${LIBCURSES} ${LIBKADM5CLNT} ${LIBKRB5} \
	${LIBCRYPTO} ${LIBDES}
LDADD+=	-lreadline -lcurses -lkadm5clnt -lkrb5 \
	-lcrypto -ldes

CLEANFILES+=	ktutil-commands.c slc slc-lex.o \
		ktutil-commands.h slc-gram.o

.include <bsd.prog.mk>
