#	$OpenBSD: Makefile,v 1.2 1999/04/30 01:59:02 art Exp $
PROG	= afsd
MAN	= afsd.8
BINDIR	= /usr/libexec
SRCS	= adir.c arla.c arladeb.c cmcb.c conn.c cred.c darla.c discon_log.c \
	  fbuf.c fcache.c fprio.c inter.c kernel.c messages.c reconnect.c \
	  volcache.c bsd-subr.c

.include "../Applflags.inc"

.PATH: ${.CURDIR}/../src/arlad

.include <bsd.prog.mk>