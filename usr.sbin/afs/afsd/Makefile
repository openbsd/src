#	$OpenBSD: Makefile,v 1.1.1.1 1998/09/14 21:53:33 art Exp $
PROG	= afsd
MAN	= afsd.8
BINDIR	= /usr/libexec
SRCS	= adir.c arla.c arladeb.c cmcb.c conn.c cred.c fbuf.c fcache.c \
	  fprio.c inter.c kernel.c messages.c volcache.c bsd-subr.c

.include "../Applflags.inc"

.PATH: ${.CURDIR}/../src/arlad

.include <bsd.prog.mk>