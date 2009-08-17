# $OpenBSD: Makefile,v 1.4 2009/08/17 09:40:37 jsg Exp $

AFSTOPDIR	= ../..
.include <../../Makefile.inc>

.PATH:	$(ROKENSRC)

PROG	= fs
BINDIR	= /usr/sbin
LDADD	+= ${LIBARLA} -lkrb5 -lcrypto -lasn1 -ledit -lcurses
DPADD	+= ${LIBARLA}

CFLAGS	+= -I${AFSSRC}/appl/lib

SRCS	= fs.c \
	  fs_arladebug.c \
	  fs_checkservers.c \
	  fs_connect.c \
	  fs_copyacl.c \
	  fs_diskfree.c \
	  fs_examine.c \
	  fs_flush.c \
	  fs_flushvol.c \
	  fs_getcache.c \
	  fs_getcellstat.c \
	  fs_getfid.c \
	  fs_getprio.c \
	  fs_la.c \
	  fs_listcell.c \
	  fs_listquota.c \
	  fs_lsm.c \
	  fs_mkm.c \
	  fs_newcell.c \
	  fs_nnpfsdebug.c \
	  fs_quota.c \
	  fs_rmm.c \
	  fs_setacl.c \
	  fs_setcache.c \
	  fs_setcrypt.c \
	  fs_setquota.c \
	  fs_statistics.c \
	  fs_strerror.c \
	  fs_whereis.c \
	  fs_whichcell.c \
	  fs_wscell.c \
	  estrdup.c \
	  print_version.c \
	  getarg.c

.PATH: ${AFSSRC}/appl/fs

SRCS	+= arladebu.c fs_lib.c

.PATH: ${AFSSRC}/appl/lib
.PATH: ${AFSSRC}/arlad

.include <bsd.prog.mk>
