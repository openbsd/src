#	$OpenBSD: Makefile,v 1.5 2003/08/05 11:11:42 hin Exp $

AFSTOPDIR=	../..
.include <../../Makefile.inc>

.PATH:	$(ROKENSRC)

PROG	= afsd
MAN	= afsd.8 CellServDB.5 ThisCell.5 SuidCells.5 afsd.conf.5 DynRootDB.5
BINDIR	= /usr/libexec
LDADD	+= ${LIBARLA} -lkrb5 -lasn1 -lcrypto
DPADD	+= ${LIBARLA}

CFLAGS	+=  -I${AFSSRC}/util -I${AFSSRC}/arlad \
	   -I${AFSSRC} -I${DESTDIR}/usr/include/kerberosIV

SRCS	= adir.c \
	  arla.c \
	  arlad.c \
	  arladeb.c \
	  arladebu.c \
	  bsd-subr.c \
	  cmcb.c \
	  conn.c \
	  cred.c \
	  d-trans.c \
	  disco.c \
	  dynroot.c \
	  fcache.c \
	  fprio.c \
	  inter.c \
	  kernel.c \
	  messages.c \
	  nnpfs.c \
	  poller.c \
	  state.c \
	  stats.c \
	  subr.c \
	  volcache.c \
	  estrdup.c \
	  erealloc.c \
	  print_version.c

.PATH: ${AFSSRC}/arlad

afsd.8:
	ln -s ${AFSSRC}/arlad/arlad.8 afsd.8
afsd.conf.5:
	ln -s ${AFSSRC}/arlad/arla.conf.5 afsd.conf.5

CLEANFILES+=afsd.8 fbuf.h fdir.h afs_dir.h afsd.conf.5

.include <bsd.prog.mk>
