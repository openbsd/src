# $OpenBSD: Makefile,v 1.1 2002/06/07 05:02:58 hin Exp $

AFSTOPDIR	= ../..
.include <../../Makefile.inc>

PROG	= bos
BINDIR	= /usr/sbin
MAN	= bos.8
LDADD	+= ${LIBARLA} ${LIBROKEN} -lkafs -lkrb -lreadline -ldes -lcurses
DPADD	+= ${LIBARLA} ${LIBROKEN} ${LIBKAFS}

CFLAGS	+= -I${AFSSRC}/appl/lib

SRCS	= bos.c \
	  bos_adduser.c \
	  bos_getrestart.c \
	  bos_listhosts.c \
	  bos_listusers.c \
	  bos_status.c 

.PATH: ${AFSSRC}/appl/bos

#SRCS	+= arladebu.c fs_lib.c

.PATH: ${AFSSRC}/appl/lib
.PATH: ${AFSSRC}/arlad

.include <bsd.prog.mk>
