# $OpenBSD: Makefile,v 1.2 2002/05/11 00:20:18 espie Exp $

PROG	= kftp

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/appl/ftp/common ${KRBSRC}/appl/ftp/ftp

CFLAGS	+= -I. -I${KRBSRC}/appl/ftp/common
DPADD	+= ${LIBKRB} ${LIBKAFS} ${LIBDES} ${LIBREADLINE} ${LIBCURSES}
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes -lreadline -lcurses

SRCS	= sockbuf.c buffer.c common.h cmds.c cmdtab.c extern.h \
	ftp.c ftp_locl.h ftp_var.h main.c pathnames.h ruserpass.c \
	domacro.c globals.c security.c security.h krb4.c kauth.c

MAN	= ftp.1

.include <bsd.prog.mk>
