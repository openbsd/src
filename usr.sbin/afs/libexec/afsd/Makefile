#	$OpenBSD: Makefile,v 1.4 2002/06/11 14:30:32 brad Exp $

AFSTOPDIR=	../..
.include <../../Makefile.inc>

PROG	= afsd
MAN	= afsd.8 CellServDB.5 ThisCell.5 SuidCells.5 afsd.conf.5 DynRootDB.5
BINDIR	= /usr/libexec
LDADD	+= ${LIBARLA} ${LIBROKEN} -lkafs -lkrb -lkrb5 -lasn1 -lcrypto -ldes
DPADD	+= ${LIBARLA} ${LIBROKEN}

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
	  darla.c \
	  discon_log.c \
	  dynroot.c \
	  fcache.c \
	  fprio.c \
	  inter.c \
	  kernel.c \
	  messages.c \
	  reconnect.c \
	  subr.c \
	  volcache.c \
	  xfs.c

.PATH: ${AFSSRC}/arlad

afsd.8:
	ln -s ${AFSSRC}/arlad/arlad.8 afsd.8
afsd.conf.5:
	ln -s ${AFSSRC}/arlad/arla.conf.5 afsd.conf.5

CLEANFILES+=afsd.8 fbuf.h fdir.h afs_dir.h afsd.conf.5

.include <bsd.prog.mk>
