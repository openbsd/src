#	$OpenBSD: Makefile,v 1.16 2000/06/23 16:15:49 espie Exp $

PROG=	make
CFLAGS+= -I${.CURDIR} -Wall -Wno-char-subscripts -Wno-unused -Wstrict-prototypes#-Wmissing-prototypes -Wstrict-prototypes

.if (${MACHINE_ARCH} == "m88k")
CFLAGS+=-O0
.endif

SRCS=	arch.c buf.c compat.c cond.c dir.c error.c for.c hash.c job.c main.c \
	make.c parse.c str.c suff.c targ.c var.c #util.c 
SRCS+=	lstAppend.c lstAtEnd.c lstAtFront.c lstClose.c lstConcat.c \
	lstConcatDestroy.c lstDeQueue.c lstDestroy.c lstDupl.c lstEnQueue.c \
	lstFindFrom.c lstForEachFrom.c \
	lstInit.c lstInsert.c lstIsAtEnd.c lstIsEmpty.c \
	lstMember.c lstNext.c lstOpen.c lstRemove.c lstReplace.c lstSucc.c
.PATH:	${.CURDIR}/lst.lib

.if make(install)
SUBDIR+= PSD.doc
.endif

.include <bsd.prog.mk>
