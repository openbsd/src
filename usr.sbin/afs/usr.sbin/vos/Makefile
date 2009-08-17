# $OpenBSD: Makefile,v 1.4 2009/08/17 09:40:37 jsg Exp $

AFSTOPDIR	= ../..
.include <../../Makefile.inc>

.PATH:	$(ROKENSRC)

PROG	= vos
MAN	= vos.8
BINDIR	= /usr/sbin
SRCS	= vos.c \
	  vos_backup.c \
	  vos_common.c \
	  vos_createentry.c \
	  vos_createvolume.c \
	  vos_dump.c \
	  vos_endtrans.c \
	  vos_examine.c \
	  vos_listaddrs.c \
	  vos_listpart.c \
	  vos_listvldb.c \
	  vos_listvol.c \
	  vos_lock.c \
	  vos_partinfo.c \
	  vos_status.c \
	  vos_syncsite.c \
	  vos_unlock.c \
	  vos_vldbexamine.c \
	  vos_zap.c \
	  emalloc.c \
	  estrdup.c

LDADD	+= ${LIBARLA} -lkrb5 -lcrypto -lasn1 -ledit -lcurses
DPADD	+= ${LIBARLA}

.PATH: ${AFSSRC}/appl/vos

CFLAGS	+= -I${AFSSRC}/appl/lib -I../libarla
.PATH: ${AFSSRC}/appl/lib
.PATH: ${AFSSRC}/arlad

.include <bsd.prog.mk>
