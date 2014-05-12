# $OpenBSD: Makefile.inc,v 1.1 2014/05/12 19:11:20 espie Exp $

.PATH:	${.CURDIR}/lst.lib
CFLAGS += -I${.CURDIR}/lst.lib
HOSTCFLAGS += -I${.CURDIR}/lst.lib

SRCS+=	lstAddNew.c lstAppend.c lstConcat.c lstConcatDestroy.c \
	lstDeQueue.c lstDestroy.c lstDupl.c lstFindFrom.c lstForEachFrom.c \
	lstInsert.c lstMember.c lstRemove.c lstReplace.c lstRequeue.c lstSucc.c
