#	$OpenBSD: Makefile,v 1.3 2000/04/27 22:49:05 hin Exp $
PROG	= afsd
MAN	= afsd.8 CellServDB.5 ThisCell.5
BINDIR	= /usr/libexec
SRCS	= adir.c arla.c arladeb.c cmcb.c conn.c cred.c darla.c discon_log.c \
	  fbuf.c fcache.c fprio.c inter.c kernel.c messages.c reconnect.c \
	  volcache.c bsd-subr.c

.include "../Applflags.inc"

.PATH: ${.CURDIR}/../src/arlad

.include <bsd.prog.mk>