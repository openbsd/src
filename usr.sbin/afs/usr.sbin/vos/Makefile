# $OpenBSD: Makefile,v 1.1 2002/06/07 05:02:59 hin Exp $

AFSTOPDIR	= ../..
.include <../../Makefile.inc>

PROG	= vos
MAN	= vos.8
BINDIR	= /usr/sbin
SRCS	= vos.c vos_common.c vos_createentry.c vos_createvolume.c \
	  vos_dump.c vos_endtrans.c vos_examine.c vos_listpart.c \
	  vos_listvldb.c vos_listvol.c vos_lock.c vos_partinfo.c \
	  vos_status.c vos_syncsite.c vos_unlock.c vos_vldbexamine.c \
	  vos_backup.c

LDADD	+= ${LIBARLA} ${LIBROKEN} -lkafs -lkrb -lreadline -ldes -lcurses
DPADD	+= ${LIBARLA} ${LIBROKEN} ${LIBKAFS}

.PATH: ${AFSSRC}/appl/vos

CFLAGS	+= -I${AFSSRC}/appl/lib -I../libarla
.PATH: ${AFSSRC}/appl/lib
.PATH: ${AFSSRC}/arlad

.include <bsd.prog.mk>
