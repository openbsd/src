#	$OpenBSD: Makefile,v 1.5 2000/09/11 14:40:26 art Exp $
PROG	= afsd
MAN	= afsd.8 CellServDB.5 ThisCell.5 SuidCells.5
BINDIR	= /usr/libexec
SRCS	= adir.c arla.c arladeb.c cmcb.c conn.c cred.c darla.c discon_log.c \
	  fcache.c fprio.c inter.c kernel.c messages.c reconnect.c \
	  volcache.c bsd-subr.c arladeb2.c dynroot.c xfs.c subr.c
CLEANFILES += fbuf.h fdir.h afs_dir.h

.include "../Applflags.inc"

.PATH: ${.CURDIR}/../src/arlad

fbuf.h:
	ln -s ${.CURDIR}/../src/lib/bufdir/fbuf.h
fdir.h:
	ln -s ${.CURDIR}/../src/lib/bufdir/fdir.h
afs_dir.h:
	ln -s ${.CURDIR}/../src/lib/bufdir/afs_dir.h

beforedepend: fbuf.h afs_dir.h fdir.h

.include <bsd.prog.mk>
