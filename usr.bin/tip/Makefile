#	$OpenBSD: Makefile,v 1.12 2010/06/29 16:41:56 nicm Exp $
#
# Files are:
#	/etc/remote		remote host description file
#	/var/log/aculog		ACU accounting file, owned by ${OWNER} and
#				mode 6?? {if ACULOG defined}
#
# Configuration defines:
#	ACULOG			turn on tip logging of ACU use
#	CONNECT			worthless command
#	DEFBR			default baud rate to make connection at
#	DEFFS			default frame size for FTP buffering of
#				writes on local side
#	BUFSIZ			buffer sizing from stdio, must be fed
#				explicitly to remcap.c if not 1024
#	CONNECT			enable ~C command (connect pgm to remote)

PROG=	tip
LINKS=	${BINDIR}/tip ${BINDIR}/cu
MAN=	tip.1 cu.1
CFLAGS+=-I${.CURDIR} -DDEFBR=9600 -DDEFFS=BUFSIZ -DACULOG -DCONNECT
SRCS=	cmds.c cmdtab.c cu.c hunt.c log.c partab.c remote.c tip.c \
	tipout.c uucplock.c value.c vars.c

.include <bsd.prog.mk>
