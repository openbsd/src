#	$OpenBSD: Makefile,v 1.5 1996/11/30 21:08:48 millert Exp $
#	$NetBSD: Makefile,v 1.12 1996/11/06 17:58:58 christos Exp $
#	@(#)Makefile	5.2 (Berkeley) 12/28/90

PROG=	make
CFLAGS+= -I${.CURDIR} -Wall -Wno-unused #-Wmissing-prototypes -Wstrict-prototypes
SRCS=	arch.c buf.c compat.c cond.c dir.c for.c hash.c job.c main.c \
	make.c parse.c str.c suff.c targ.c var.c util.c
SRCS+=	lstAppend.c lstAtEnd.c lstAtFront.c lstClose.c lstConcat.c \
	lstDatum.c lstDeQueue.c lstDestroy.c lstDupl.c lstEnQueue.c \
	lstFind.c lstFindFrom.c lstFirst.c lstForEach.c lstForEachFrom.c \
	lstInit.c lstInsert.c lstIsAtEnd.c lstIsEmpty.c lstLast.c \
	lstMember.c lstNext.c lstOpen.c lstRemove.c lstReplace.c lstSucc.c
.PATH:	${.CURDIR}/lst.lib

.if make(install)
SUBDIR+= PSD.doc
.endif

.include <bsd.prog.mk>
